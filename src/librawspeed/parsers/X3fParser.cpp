/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2020 Yanan Wen

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "parsers/X3fParser.h"
#include "decoders/X3fDecoder.h"         // for X3fDecoder
#include "decoders/RawDecoder.h"         // for RawDecoder
#include "io/Buffer.h"                   // for Buffer, DataBuffer
#include "io/ByteStream.h"               // for ByteStream
#include "io/Endianness.h"               // for Endianness, Endianness::big
#include "parsers/FiffParserException.h" // for ThrowFPE
#include "parsers/RawParser.h"           // for RawParser
#include "parsers/TiffParser.h"          // for TiffParser
#include "parsers/TiffParserException.h" // for TiffParserException
#include "tiff/TiffEntry.h"              // for TiffEntry, TIFF_SHORT, TIFF...
#include "tiff/TiffIFD.h"                // for TiffIFD, TiffRootIFDOwner
#include "tiff/TiffTag.h"                // for FUJIOLDWB, FUJI_STRIPBYTECO...
#include "X3fParserException.h"
#include <cstdint>                       // for uint32_t, uint16_t
#include <string>                        // for string
#include <limits>                        // for numeric_limits
#include <memory>                        // for make_unique, unique_ptr
#include <utility>                       // for move

using std::numeric_limits;
using std::string;

namespace rawspeed {

/* Main file identifier */
#define X3F_FOVb (uint32_t)(0x62564f46)
/* Directory identifier */
#define X3F_SECd (uint32_t)(0x64434553)
/* Property section identifiers */
#define X3F_PROP (uint32_t)(0x504f5250)
#define X3F_SECp (uint32_t)(0x70434553)
/* Image section identifiers */
#define X3F_IMAG (uint32_t)(0x46414d49)
#define X3F_IMA2 (uint32_t)(0x32414d49)
#define X3F_SECi (uint32_t)(0x69434553)
/* CAMF section identifiers */
#define X3F_CAMF (uint32_t)(0x464d4143)
#define X3F_SECc (uint32_t)(0x63434553)
/* CAMF entry identifiers */
#define X3F_CMbP (uint32_t)(0x50624d43)
#define X3F_CMbT (uint32_t)(0x54624d43)
#define X3F_CMbM (uint32_t)(0x4d624d43)
#define X3F_CMb  (uint32_t)(0x00624d43)
/* SDQ section identifiers ? - TODO */
#define X3F_SPPA (uint32_t)(0x41505053)
#define X3F_SECs (uint32_t)(0x73434553)


#define X3F_VERSION(MAJ,MIN) (uint32_t)(((MAJ)<<16) + MIN)
#define X3F_VERSION_2_0 X3F_VERSION(2,0)
#define X3F_VERSION_2_1 X3F_VERSION(2,1)
#define X3F_VERSION_2_2 X3F_VERSION(2,2)
#define X3F_VERSION_2_3 X3F_VERSION(2,3)
#define X3F_VERSION_3_0 X3F_VERSION(3,0)
#define X3F_VERSION_4_0 X3F_VERSION(4,0)
#define X3F_VERSION_4_1 X3F_VERSION(4,1)


X3fParser::X3fParser(const Buffer* input) : RawParser(input) {
    // check file size
    size_t size = input->getSize();
    if (size < 104 + 128) {
        ThrowXPE("X3F file too small");
    }

    // init the byte stream
    bs = ByteStream(DataBuffer(*mInput, Endianness::little));

    // parse the X3F file header
    try {
        // parse header
        X3fHeader header(bs);
    } catch (IOException& e) {
        ThrowXPE("IO Error while reading header: %s", e.what());
    }
}

std::unique_ptr<RawDecoder> X3fParser::getDecoder(const CameraMetaData* meta) {
    auto decoder = std::make_unique<X3fDecoder>(mInput);

    // extract X3F directories
    try {
        parseData(decoder.get());
    } catch (X3fParserException &e) {
        ThrowXPE("Parser error while preparing data for decoder: %s", e.what());
    }

    return decoder;
}

void X3fParser::parseData(X3fDecoder *decoder) {
    // Go to the beginning of the directory, location stored at the end of the file, uint32_t
    bs.setPosition(bs.getSize()-4);
    // locate to the directory
    uint32_t dir_loc = bs.getU32();
    bs.setPosition(dir_loc);

    // extract directory section
    X3fDirectorySection dirSec(bs);

    // visit all entries
    for (auto i=0; i<dirSec.dirNum; ++i) {
        X3fDirectoryEntry dir(bs);

        // save current position
        uint32_t old_pos = bs.getPosition();

        // seek to the directory
        bs.setPosition(dir.offset);

        switch (dir.type) {
            case X3F_IMAG:
            case X3F_IMA2: {
                // image entry, add to decoder
                decoder->images.emplace_back(bs);
                break;
            }
            case X3F_PROP: {
                // property entry, add to decoder
                decoder->properties.addProperties(bs, dir.offset);
                break;
            }
            case X3F_CAMF: {
                decoder->camf = X3fCamf(bs);
                break;
            }
            default: {
                // do nothing
            }
        }

        bs.setPosition(old_pos);
    }
}

X3fSection::X3fSection(ByteStream &bs) {
    id      = bs.getU32();
    version = bs.getU32();
}

X3fHeader::X3fHeader(ByteStream &bs): X3fSection(bs) {
    /**
     * Header Section
     * Version 2.1-2.2
     * 4   bytes, file type identifier, contains "FOVb", used to verify that this is an FOVb file.
     * 4   bytes, file format version, version of the file.
     * 16  bytes, unique identifier, guaranteed unique to each image. Formed from camera serial number / OUI, timestamp, and high-speed timer register. Can be used to identify images even if they are renamed. No, it's not UUID-compatible.
     * 4   bytes, mark bits, can be used to denote that images are marked into one or more subsets. File interface functions allow setting these bits and searching for files based on these bits. This feature will not be usable on write- once media.
     * 4   bytes, image columns, width of unrotated image in columns. This is the size output the user expects from this image, not the size of the raw image data. Not necessarily equal to the width of any image entry in the file; this supports images where the raw data has rectangular pixels.
     * 4   bytes, image rows, height of unrotated image in rows. This is the size output the user expects from this image, not the size of the raw image data. Not necessarily equal to the width of any image entry in the file; this supports images where the raw data has rectangular pixels.
     * 4   bytes, rotation, image rotation in degrees clockwise from normal camera orientation. Valid values are 0, 90, 180, 270.
     * 32  bytes, white balance label string, contains an ASCIIZ string label of the current white balance setting for this image.
     * 32  bytes, extended data types, contains 32 8-bit values indicating the types of the following extended data.
     * 128 bytes, extended data, contains 32 32-bit values of extended data.
     */
    if (id != X3F_FOVb) {
        ThrowXPE("Not an X3f file (Signature)");
    }

    // get the unique identifier
    for (auto i=0; i<=SIZE_UNIQUE_IDENTIFIER-1; ++i) {
        unique_identifier[i] = bs.getByte();
    }

    // the meaning of the rest of the header for version >= 4.0 (Quattro) is unknown
    // 20 bytes for mark and other bits
    // bytes.skipBytes(16 + 4);
    if (version < X3F_VERSION_4_0) {
        mark_bits = bs.getU32();
        columns   = bs.getU32();
        rows      = bs.getU32();
        rotation  = bs.getU32();

        if (version >= X3F_VERSION_2_1) {
            int num_ext_data = version >= X3F_VERSION_3_0 ? NUM_EXT_DATA_3_0 : NUM_EXT_DATA_2_1;

            for (auto i=0; i<=SIZE_WHITE_BALANCE-1; ++i) {
                white_balance[i] = bs.getByte();
            }

            if (version >= X3F_VERSION_2_3) {
                for (auto i=0; i<=SIZE_WHITE_BALANCE-1; ++i) {
                    color_mode[i] = bs.getByte();
                }
            }

            for (auto i=0; i<=num_ext_data; ++i) {
                extended_types[i] = bs.getByte();
            }

            for (auto i=0; i<num_ext_data; i++) {
                extended_data[i] = bs.getFloat();
            }
        }
    }
}

X3fDirectorySection::X3fDirectorySection(ByteStream &bs): X3fSection(bs) {
    /**
     * Data Section
     * Directory Section
     * Directory section always starts with the following data:
     * 4 bytes, section identifier, contains "SECd"
     * 4 bytes, section version, should be 2.0
     * 4 bytes, number of directory entries
     */
    if ((id!=X3F_SECd) && (id!=X3F_SECc)) {
        ThrowXPE("Unknown X3F directory identifier");
    }

    if (version < X3F_VERSION_2_0) {
        ThrowXPE("X3F version older than 2.0 is not supported");
    }

    dirNum = bs.getU32();
    if (dirNum < 1) {
        ThrowXPE("X3F directory is empty");
    }
}

    static string getIdAsString(ByteStream &bytes) {
        uint8_t id[5];
        for (int i = 0; i < 4; i++)
            id[i] = bytes.getByte();
        id[4] = 0;
        return string(reinterpret_cast<const char*>(id));
    }


X3fDirectoryEntry::X3fDirectoryEntry(ByteStream &bs) {
    /**
     * Directory Entry
     * 4 bytes, offset from start of file to start of entry's data, in bytes. Offset must be a multiple of 4, so that the data starts on a 32-bit boundary.
     * 4 bytes, length of entry's data, in bytes.
     * 4 bytes, type of entry.
     */
    offset = bs.getU32();
    length = bs.getU32();
    type   = bs.getU32();

    uint32_t old_pos = bs.getPosition();

    bs.setPosition(offset);
    _sectionID = bs.getU32();

    bs.setPosition(old_pos);
}

X3fImageData::X3fImageData(ByteStream &bs): X3fSection(bs) {
    type     = bs.getU32();
    format   = bs.getU32();
    width    = bs.getU32();
    height   = bs.getU32();
    dataSize = bs.getU32();
}

X3fPropertyList::X3fPropertyList(ByteStream &bs): X3fSection(bs) {
    num      = bs.getU32();
    format   = bs.getU32();
    reserved = bs.getU32();
    length   = bs.getU32();
}

X3fPropertyEntry::X3fPropertyEntry(ByteStream &bs) {
    key_off = bs.getU32();
    val_off = bs.getU32();
}

X3fCamf::X3fCamf(ByteStream &bs) {
    type = bs.getU32();
    tN.val0 = bs.getU32();
    tN.val1 = bs.getU32();
    tN.val2 = bs.getU32();
    tN.val3 = bs.getU32();
}

/*
* ConvertUTF16toUTF8 function only Copyright:
*
* Copyright 2001-2004 Unicode, Inc.
*
* Disclaimer
*
* This source code is provided as is by Unicode, Inc. No claims are
* made as to fitness for any particular purpose. No warranties of any
* kind are expressed or implied. The recipient agrees to determine
* applicability of information provided. If this file has been
* purchased on magnetic or optical media from Unicode, Inc., the
* sole remedy for any claim will be exchange of defective media
* within 90 days of receipt.
*
* Limitations on Rights to Redistribute This Code
*
* Unicode, Inc. hereby grants the right to freely use the information
* supplied in this file in the creation of products supporting the
* Unicode Standard, and to make copies of this file in any form
* for internal or external distribution as long as this notice
* remains attached.
*/

    using UTF32 = unsigned int;    /* at least 32 bits */
    using UTF16 = unsigned short;  /* at least 16 bits */
    using UTF8 = unsigned char;    /* typically 8 bits */
    using Boolean = unsigned char; /* 0 or 1 */

/* Some fundamental constants */
#define UNI_REPLACEMENT_CHAR (UTF32)0x0000FFFD
#define UNI_MAX_BMP (UTF32)0x0000FFFF
#define UNI_MAX_UTF16 (UTF32)0x0010FFFF
#define UNI_MAX_UTF32 (UTF32)0x7FFFFFFF
#define UNI_MAX_LEGAL_UTF32 (UTF32)0x0010FFFF

#define UNI_MAX_UTF8_BYTES_PER_CODE_POINT 4

#define UNI_UTF16_BYTE_ORDER_MARK_NATIVE  0xFEFF
#define UNI_UTF16_BYTE_ORDER_MARK_SWAPPED 0xFFFE

#define UNI_SUR_HIGH_START  (UTF32)0xD800
#define UNI_SUR_HIGH_END    (UTF32)0xDBFF
#define UNI_SUR_LOW_START   (UTF32)0xDC00
#define UNI_SUR_LOW_END     (UTF32)0xDFFF

static const int halfShift  = 10; /* used for shifting by 10 bits */
static const UTF32 halfBase = 0x0010000UL;
static const UTF8 firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

static bool ConvertUTF16toUTF8(const UTF16** sourceStart,
                               const UTF16* sourceEnd, UTF8** targetStart,
                               const UTF8* targetEnd) {
    bool success = true;
    const UTF16* source = *sourceStart;
    UTF8* target = *targetStart;
    while (source < sourceEnd) {
        UTF32 ch;
        unsigned short bytesToWrite = 0;
        const UTF32 byteMask = 0xBF;
        const UTF32 byteMark = 0x80;
        const UTF16* oldSource = source; /* In case we have to back up because of target overflow. */
        ch = *source;
        source++;
        /* If we have a surrogate pair, convert to UTF32 first. */
        if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
            /* If the 16 bits following the high surrogate are in the source buffer... */
            if (source < sourceEnd) {
                UTF32 ch2 = *source;
                /* If it's a low surrogate, convert to UTF32. */
                if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
                    ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
                         + (ch2 - UNI_SUR_LOW_START) + halfBase;
                    ++source;
#if 0
                    } else if (flags == strictConversion) { /* it's an unpaired high surrogate */
      --source; /* return to the illegal value itself */
      success = false;
      break;
#endif
                }
            } else { /* We don't have the 16 bits following the high surrogate. */
                --source; /* return to the high surrogate */
                success = false;
                break;
            }
        }
        /* Figure out how many bytes the result will require */
        if (ch < static_cast<UTF32>(0x80)) {
            bytesToWrite = 1;
        } else if (ch < static_cast<UTF32>(0x800)) {
            bytesToWrite = 2;
        } else if (ch < static_cast<UTF32>(0x10000)) {
            bytesToWrite = 3;
        } else if (ch < static_cast<UTF32>(0x110000)) {
            bytesToWrite = 4;
        } else {                            bytesToWrite = 3;
            ch = UNI_REPLACEMENT_CHAR;
        }

        target += bytesToWrite;
        if (target > targetEnd) {
            source  = oldSource; /* Back up source pointer! */
            target -= bytesToWrite;
            success = false;
            break;
        }
        assert(bytesToWrite > 0);
        for (int i = bytesToWrite; i > 1; i--) {
            target--;
            *target = static_cast<UTF8>((ch | byteMark) & byteMask);
            ch >>= 6;
        }
        target--;
        *target = static_cast<UTF8>(ch | firstByteMark[bytesToWrite]);
        target += bytesToWrite;
    }
    // Function modified to retain source + target positions
    //  *sourceStart = source;
    //  *targetStart = target;
    return success;
}

string X3fPropertyCollection::getString(ByteStream &bs) {
    uint32_t max_len = bs.getRemainSize() / 2;
    const auto* start =
            reinterpret_cast<const UTF16*>(bs.getData(max_len * 2));
    const UTF16* src_end = start;
    uint32_t i = 0;
    for (; i < max_len && start == src_end; i++) {
        if (start[i] == 0) {
            src_end = &start[i];
        }
    }
    if (start != src_end) {
        auto *dest = new UTF8[i * 4UL + 1];
        memset(dest, 0, i * 4UL + 1);
        if (ConvertUTF16toUTF8(&start, src_end, &dest, &dest[i * 4 - 1])) {
            string ret(reinterpret_cast<const char*>(dest));
            delete[] dest;
            return ret;
        }
        delete[] dest;
    }
    return "";
}

void X3fPropertyCollection::addProperties(ByteStream &bs, uint32_t offset) {
    bs.setPosition(offset);

    X3fPropertyList pl(bs);

    if (pl.id != X3F_SECp)
        ThrowXPE("Unknown Property signature");

    if (pl.version < X3F_VERSION_2_0)
        ThrowXPE("File version too old (properties)");

    if (pl.num < 1)
        return;

    if (pl.format != 0)
        ThrowXPE("Unknown property character encoding");

    if (pl.num > 1000)
        ThrowXPE("Unreasonable number of properties: %u", pl.num);

    uint32_t data_start = bs.getPosition() + pl.num*8;

    for (uint32_t i = 0; i < pl.num; i++) {
        X3fPropertyEntry pe(bs);

        uint32_t old_pos = bs.getPosition();

        if (    bs.isValid(pe.key_off * 2 + data_start, 2)
            &&  bs.isValid(pe.val_off * 2 + data_start, 2)) {

            bs.setPosition(pe.key_off * 2 + data_start);
            string key = getString(bs);
            bs.setPosition(pe.val_off * 2 + data_start);
            string val = getString(bs);
            props[key] = val;
        }

        bs.setPosition(old_pos);
    }
}

} // namespace rawspeed
