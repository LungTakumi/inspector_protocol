// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_JSON_PARSER_H_
#define INSPECTOR_PROTOCOL_ENCODING_JSON_PARSER_H_

#include <cstdint>
#include <vector>
#include "json_parser_handler.h"
#include "span.h"
#include "system_deps.h"

namespace inspector_protocol {
// JSON parsing routines.
void parseJSONChars(const SystemDeps* deps, span<uint8_t> chars,
                    JsonParserHandler* handler);
void parseJSONChars(const SystemDeps* deps, span<uint16_t> chars,
                    JsonParserHandler* handler);
}  // namespace inspector_protocol

#endif  // INSPECTOR_PROTOCOL_ENCODING_JSON_PARSER_H_
