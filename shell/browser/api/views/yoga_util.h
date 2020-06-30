// Copyright (c) 2020 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef SHELL_BROWSER_API_VIEWS_YOGA_UTIL_H_
#define SHELL_BROWSER_API_VIEWS_YOGA_UTIL_H_

#include <string>

typedef struct YGNode* YGNodeRef;

namespace nu {

void SetYogaProperty(YGNodeRef node, const std::string& key, float value);
void SetYogaProperty(YGNodeRef node,
                     const std::string& key,
                     const std::string& value);

}  // namespace nu

#endif  // SHELL_BROWSER_API_VIEWS_YOGA_UTIL_H_
