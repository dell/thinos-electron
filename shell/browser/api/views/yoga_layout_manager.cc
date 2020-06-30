// Copyright (c) 2020 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/api/views/yoga_layout_manager.h"

#include "shell/browser/api/views/yoga_util.h"
#include "third_party/yoga/yoga/Yoga.h"
#include "ui/views/view.h"

namespace electron {

YogaLayoutManager::YogaLayoutManager() {}

YogaLayoutManager::~YogaLayoutManager() {}

void YogaLayoutManager::Layout(views::View* host) {
  YGNodeRef node = GetYogaNode(host);
  if (!node)
    return;

  YGNodeCalculateLayout(node, host->width(), host->height(), YGDirectionLTR);
  for (auto* child : host->children()) {
    if (!child->GetVisible())
      continue;
    YGNodeRef child_node = GetYogaNode(child);
    if (!child_node)
      continue;
    child->SetBounds(
        YGNodeLayoutGetLeft(child_node), YGNodeLayoutGetTop(child_node),
        YGNodeLayoutGetWidth(child_node), YGNodeLayoutGetHeight(child_node));
    child->Layout();
  }
}

gfx::Size YogaLayoutManager::GetPreferredSize(const views::View* host) const {
  YGNodeRef node = GetYogaNode(host);
  if (!node)
    return gfx::Size();

  float nan = std::numeric_limits<float>::quiet_NaN();
  YGNodeCalculateLayout(node, nan, nan, YGDirectionLTR);
  return gfx::Size(YGNodeLayoutGetWidth(node), YGNodeLayoutGetHeight(node));
}

}  // namespace electron
