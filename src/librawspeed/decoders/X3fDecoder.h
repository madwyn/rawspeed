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

#pragma once

#include "common/RawImage.h"              // for RawImage
#include "decoders/AbstractTiffDecoder.h" // for AbstractTiffDecoder
#include "io/ByteStream.h"                // for ByteStream
#include "tiff/TiffIFD.h"                 // for TiffIFD (ptr only), TiffRo...
#include <cstdint>                        // for uint32_t
#include <utility>                        // for move

namespace rawspeed {

class CameraMetaData;
class Buffer;


class X3fSection {
public:
    explicit X3fSection(uint32_t offset, ByteStream &bytes);
    uint32_t id;        // section identifier
    uint32_t version;   // section version
    uint32_t _offset;   // offset from the start of the file, this is not a property field
};

// X3F header macros
#define SIZE_UNIQUE_IDENTIFIER 16
#define SIZE_WHITE_BALANCE 32
#define SIZE_COLOR_MODE 32
#define NUM_EXT_DATA_2_1 32
#define NUM_EXT_DATA_3_0 64
#define NUM_EXT_DATA NUM_EXT_DATA_3_0
class X3fHeader: public X3fSection {
public:
    explicit X3fHeader(ByteStream &bytes);
    /* 2.0 Fields */
    // uint32_t id;          /* Should be �FOVb� 0x62564f46 */
    // uint32_t version;     /* 0x00020001 means 2.1 */
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
};

class X3fDirectorySection : public X3fSection {
public:
    explicit X3fDirectorySection(ByteStream &bytes);
    // uint32_t id;         // section identifier, contains "SECd"
    // uint32_t version;    // section version
    uint32_t dirNum;        // number of directories
};

class X3fDirectoryEntry {
public:
    explicit X3fDirectoryEntry(ByteStream &bytes);
    uint32_t offset{0};     // offset from start of file to start of entry's data, in bytes. Offset must be a multiple of 4, so that the data starts on a 32-bit
    uint32_t length{0};     // length of entry's data, in bytes.
    uint32_t type;          // type of entry.
                            // "PROP", list of pairs of strings. Each pair is a name and its corresponding value.
                            // "IMAG", image data. Has a header indicating dimensions, pixel type, compression, amount of processing done.
                            // "IMA2", image data. Readers should treat this the same as IMAG. Writers should use this for image sections that contain processed-for-preview data in other than uncompressed RGB24 pixel format.
    uint32_t _sectionID;    // DataSubsection ID from X3fImageData or X3fPropertyList
};

class X3fImageData : public X3fSection {
public:
    explicit X3fImageData(uint32_t offset, ByteStream &bytes);
    // uint32_t id;        // section identifier, should be "SECi"
    // uint32_t version;   // image format version
    uint32_t type;      // 2 = processed for preview
                        // (others RESERVED)
    uint32_t format;    // 3  = uncompressed 24-bit 8/8/8 RGB
                        // 11 = Huffman-encoded DPCM 8/8/8 RGB
                        // 18 = JPEG-compressed 8/8/8 RGB
                        // (others RESERVED)
    uint32_t width;
    uint32_t height;
    uint32_t dataSize;  // Will always be a multiple of 4 (32-bit aligned). A value of zero here means that rows are variable-length (as in Huffman data).
};

class X3fPropertyList : public X3fSection {
public:
    explicit X3fPropertyList(uint32_t offset, ByteStream &bytes);
    // uint32_t id;        // section identifier, should be "SECp"
    // uint32_t version;   // property format version
    uint32_t num;       // number of property entries
    uint32_t format;    // character format for all entries in this table, 0=CHAR16 Unicode
    uint32_t reserved;
    uint32_t length;    // total length of name/value data in characters.
};

class X3fPropertyEntry {
public:
    explicit X3fPropertyEntry(ByteStream &bytes);
    uint32_t nameOffset;   // Offset in characters of property name from start of character data. Not necessarily immediately following the previous property's value.
    uint32_t valueOffset;  // Offset in characters of property value from start of character data.
};

class X3fDecoder final : public RawDecoder
{
public:
    static bool isX3f(const Buffer* input);
    static bool isAppropriateDecoder(const Buffer* file);
    explicit X3fDecoder(const Buffer* file) : RawDecoder(file) {}
    RawImage decodeRawInternal() override;
    void checkSupportInternal(const CameraMetaData* meta) override;
    void decodeMetaDataInternal(const CameraMetaData* meta) override;

protected:
    int getDecoderVersion() const override { return 0; }


};

} // namespace rawspeed
