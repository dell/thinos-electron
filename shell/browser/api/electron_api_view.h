// Copyright (c) 2018 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef SHELL_BROWSER_API_ELECTRON_API_VIEW_H_
#define SHELL_BROWSER_API_ELECTRON_API_VIEW_H_

#include <map>
#include <memory>
#include <vector>

#include "electron/buildflags/buildflags.h"
#include "gin/handle.h"
#include "shell/common/gin_helper/wrappable.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

#if BUILDFLAG(ENABLE_VIEWS_API)
typedef struct YGNode* YGNodeRef;
typedef struct YGConfig* YGConfigRef;
#endif

namespace electron {

namespace api {

class View : public gin_helper::Wrappable<View>, public views::ViewObserver {
 public:
  static gin_helper::WrappableBase* New(gin::Arguments* args);

  static void BuildPrototype(v8::Isolate* isolate,
                             v8::Local<v8::FunctionTemplate> prototype);

#if BUILDFLAG(ENABLE_VIEWS_API)
  void AddChildView(gin::Handle<View> view);
  void AddChildViewAt(gin::Handle<View> view, size_t index);

  void SetStyle(std::map<std::string, base::Value> dict);
#endif

  views::View* view() const { return view_; }

 protected:
  explicit View(views::View* view);
  View();
  ~View() override;

#if BUILDFLAG(ENABLE_VIEWS_API)
  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;
  void OnViewAddedToWidget(views::View* view) override;
#endif

  // Should delete the |view_| in destructor.
  void set_delete_view(bool should) { delete_view_ = should; }

 private:
  std::vector<v8::Global<v8::Object>> child_views_;

#if BUILDFLAG(ENABLE_VIEWS_API)
  // Yoga node.
  YGConfigRef yoga_config_;
  YGNodeRef yoga_node_;
#endif

  bool delete_view_ = true;
  views::View* view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(View);
};

}  // namespace api

}  // namespace electron

#endif  // SHELL_BROWSER_API_ELECTRON_API_VIEW_H_
