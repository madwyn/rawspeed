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

#include "parsers/RawParser.h"      // for RawParser
#include "decoders/X3fDecoder.h"    // for X3fDecoder
#include <memory>                   // for unique_ptr


namespace rawspeed {

class Buffer;
class CameraMetaData;
class RawDecoder;

class X3fParser final : public RawParser {

public:
    explicit X3fParser(const Buffer* input);

    std::unique_ptr<RawDecoder>
    getDecoder(const CameraMetaData* meta = nullptr) override;

private:
    void parseData(X3fDecoder* decoder);
    void parseHeader();
    ByteStream bs;
};

} // namespace rawspeed
