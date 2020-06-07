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
#include <limits>                        // for numeric_limits
#include <memory>                        // for make_unique, unique_ptr
#include <utility>                       // for move

using std::numeric_limits;

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



typedef struct x3f_header_s {
    /* 2.0 Fields */
    uint32_t identifier;          /* Should be �FOVb� 0x62564f46 */
    uint32_t version;             /* 0x00020001 means 2.1 */
    uint8_t unique_identifier[SIZE_UNIQUE_IDENTIFIER];
    uint32_t mark_bits;
    uint32_t columns;             /* Columns and rows ... */
    uint32_t rows;                /* ... before rotation */
    uint32_t rotation;            /* 0, 90, 180, 270 */

    char white_balance[SIZE_WHITE_BALANCE]; /* Introduced in 2.1 */
    char color_mode[SIZE_COLOR_MODE]; /* Introduced in 2.3 */

    /* Introduced in 2.1 and extended from 32 to 64 in 3.0 */
    uint8_t extended_types[NUM_EXT_DATA]; /* x3f_extended_types_t */
    float extended_data[NUM_EXT_DATA]; /* 32 bits, but do type differ? */
} x3f_header_t;


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
    parseData(decoder.get());

    // WARNING: do *NOT* fallback to ordinary TIFF parser here!
    // All the X3F raws are '.X3F' (Sigma). Do use X3fDecoder directly.

//    try {
//        if (!X3fDecoder::isAppropriateDecoder(rootIFD.get(), mInput))
//            ThrowFPE("Not a SIGMA X3F.");
//
//        return std::make_unique<X3fDecoder>(std::move(rootIFD), mInput);
//    } catch (TiffParserException&) {
//        ThrowFPE("No decoder found. Sorry.");
//    }
    return decoder;
}

void X3fParser::parseData(X3fDecoder *decoder) {
    // Go to the beginning of the directory, location stored at the end of the file, uint32_t
    bs.setPosition(bs.getRemainSize()-4);
    // locate to the directory
    uint32_t dir_loc = bs.getU32();
    bs.setPosition(dir_loc);

    // extract directory section
    X3fDirectorySection dirSec(bs);

    // visit all entries
    for (auto i=0; i<dirSec.dirNum; ++i) {
        X3fDirectoryEntry dir(bs);

        // store all the directories? why?
        // I'm only interested in the
    }
}

X3fSection::X3fSection(uint32_t offset, ByteStream &bs) {
    _offset = offset;
    id = bs.getU32();
    version = bs.getU32();
}

X3fHeader::X3fHeader(ByteStream &bs): X3fSection(0, bs) {
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

X3fDirectorySection::X3fDirectorySection(ByteStream &bs): X3fSection(0, bs) {
    /**
     * Data Section
     * Directory Section
     * Directory section always starts with the following data:
     * 4 bytes, section identifier, contains "SECd"
     * 4 bytes, section version, should be 2.0
     * 4 bytes, number of directory entries
     */
    if (id != X3F_SECd) {
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

X3fImageData::X3fImageData(uint32_t offset, ByteStream &bs): X3fSection(offset, bs) {
    type     = bs.getU32();
    format   = bs.getU32();
    width    = bs.getU32();
    height   = bs.getU32();
    dataSize = bs.getU32();
}

X3fPropertyList::X3fPropertyList(uint32_t offset, ByteStream &bs): X3fSection(offset, bs) {
    num      = bs.getU32();
    format   = bs.getU32();
    reserved = bs.getU32();
    length   = bs.getU32();
}

X3fPropertyEntry::X3fPropertyEntry(ByteStream &bs) {
    nameOffset  = bs.getU32();
    valueOffset = bs.getU32();
}

} // namespace rawspeed
