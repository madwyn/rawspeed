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

#include "decoders/ArwDecoder.h"
#include "common/Common.h"                          // for roundDown
#include "common/Point.h"                           // for iPoint2D
#include "common/RawspeedException.h"               // for RawspeedException
#include "decoders/RawDecoderException.h"           // for ThrowRDE
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Buffer.h"                              // for Buffer, DataBuffer
#include "io/ByteStream.h"                          // for ByteStream
#include "io/Endianness.h"                          // for Endianness, Endi...
#include "metadata/Camera.h"                        // for Hints
#include "metadata/ColorFilterArray.h"              // for CFA_GREEN, CFA_BLUE
#include "tiff/TiffEntry.h"                         // for TiffEntry
#include "tiff/TiffIFD.h"                           // for TiffRootIFD, Tif...
#include "tiff/TiffTag.h"                           // for DNGPRIVATEDATA
#include "X3fDecoder.h"

#include <array>                                    // for array
#include <cassert>                                  // for assert
#include <cstring>                                  // for memcpy, size_t
#include <memory>                                   // for unique_ptr
#include <set>                                      // for set
#include <string>                                   // for operator==, string
#include <vector>                                   // for vector


namespace rawspeed {

bool X3fDecoder::isX3f(const Buffer* input) {
    static const std::array<char, 4> magic = {{'F', 'O', 'V', 'b'}};
    const unsigned char* data = input->getData(0, magic.size());
    return 0 == memcmp(data, magic.data(), magic.size());
}

bool X3fDecoder::isAppropriateDecoder(const Buffer *file) {
    return isX3f(file);
}

RawImage X3fDecoder::decodeRawInternal() {

    return mRaw;
}

void X3fDecoder::checkSupportInternal(const CameraMetaData* meta) {
    // set iso
    // set wb
    //
}
void X3fDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
    // set iso
    // set wb
    //
}

} // namespace rawspeed
