// Copyright (c) 2020 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef SHELL_BROWSER_API_VIEWS_ELECTRON_API_TEXT_VIEW_H_
#define SHELL_BROWSER_API_VIEWS_ELECTRON_API_TEXT_VIEW_H_

#include "shell/browser/api/electron_api_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/styled_label_listener.h"

namespace electron {

namespace api {

class TextView : public View, public views::StyledLabelListener {
 public:
  static gin_helper::WrappableBase* New(const base::string16& text,
                                        gin_helper::Arguments* args);

  static void BuildPrototype(v8::Isolate* isolate,
                             v8::Local<v8::FunctionTemplate> prototype);

 protected:
  explicit TextView(const base::string16& text);
  ~TextView() override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override {}

  views::StyledLabel* label() const {
    return static_cast<views::StyledLabel*>(view());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TextView);
};

}  // namespace api

}  // namespace electron

#endif  // SHELL_BROWSER_API_VIEWS_ELECTRON_API_TEXT_VIEW_H_
