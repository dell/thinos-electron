// Copyright (c) 2020 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef SHELL_BROWSER_API_VIEWS_YOGA_LAYOUT_MANAGER_H_
#define SHELL_BROWSER_API_VIEWS_YOGA_LAYOUT_MANAGER_H_

#include "ui/views/layout/layout_manager.h"

namespace electron {

class YogaLayoutManager : public views::LayoutManager {
 public:
  YogaLayoutManager();
  ~YogaLayoutManager() override;

 protected:
  // views::LayoutManager:
  void Layout(views::View* host) override;
  gfx::Size GetPreferredSize(const views::View* host) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(YogaLayoutManager);
};

}  // namespace electron

#endif  // SHELL_BROWSER_API_VIEWS_YOGA_LAYOUT_MANAGER_H_
