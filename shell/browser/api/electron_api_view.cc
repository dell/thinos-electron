// Copyright (c) 2018 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/api/electron_api_view.h"

#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/node_includes.h"

#if BUILDFLAG(ENABLE_VIEWS_API)
#include "shell/browser/api/views/yoga_layout_manager.h"
#include "shell/browser/api/views/yoga_util.h"
#include "shell/common/gin_converters/value_converter.h"
#include "third_party/yoga/yoga/Yoga.h"
#include "ui/base/layout.h"
#include "ui/views/widget/widget.h"
#endif

namespace electron {

namespace api {

namespace {

#if BUILDFLAG(ENABLE_VIEWS_API)
float GetScaleFactor(views::View* view) {
  views::Widget* widget = view->GetWidget();
  if (widget) {
    gfx::NativeView view = widget->GetNativeView();
    if (view)
      return ui::GetScaleFactorForNativeView(view);
  }
  return 1.0f;
}
#endif

}  // namespace

View::View(views::View* view) : view_(view) {
  view_->set_owned_by_client();

#if BUILDFLAG(ENABLE_VIEWS_API)
  yoga_config_ = YGConfigNew();
  yoga_node_ = YGNodeNewWithConfig(yoga_config_);
  AttachYogaNode(view, yoga_node_);

  view_->SetLayoutManager(std::make_unique<YogaLayoutManager>());
  view_->AddObserver(this);
#endif
}

View::View() : View(new views::View()) {}

View::~View() {
#if BUILDFLAG(ENABLE_VIEWS_API)
  view_->RemoveObserver(this);
#endif

  if (delete_view_)
    delete view_;

#if BUILDFLAG(ENABLE_VIEWS_API)
  YGNodeFree(yoga_node_);
  YGConfigFree(yoga_config_);
#endif
}

#if BUILDFLAG(ENABLE_VIEWS_API)
void View::AddChildView(gin::Handle<View> child) {
  AddChildViewAt(child, child_views_.size());
}

void View::AddChildViewAt(gin::Handle<View> child, size_t index) {
  if (index > child_views_.size())
    return;
  child_views_.emplace(child_views_.begin() + index,     // index
                       isolate(), child->GetWrapper());  // v8::Global(args...)
  YGNodeInsertChild(yoga_node_, child->yoga_node_, index);
  view()->AddChildViewAt(child->view(), index);
  view()->Layout();
}

void View::SetStyle(std::map<std::string, base::Value> dict) {
  for (const auto& it : dict) {
    if (it.second.is_double())
      SetYogaProperty(yoga_node_, it.first, it.second.GetDouble());
    else if (it.second.is_int())
      SetYogaProperty(yoga_node_, it.first, it.second.GetInt());
    else if (it.second.is_string())
      SetYogaProperty(yoga_node_, it.first, it.second.GetString());
  }
}

void View::OnViewVisibilityChanged(views::View* observed_view,
                                   views::View* starting_view) {
  YGNodeStyleSetDisplay(yoga_node_,
                        view()->GetVisible() ? YGDisplayFlex : YGDisplayNone);
}

void View::OnViewAddedToWidget(views::View* view) {
  YGConfigSetPointScaleFactor(yoga_config_, GetScaleFactor(view));
}
#endif

// static
gin_helper::WrappableBase* View::New(gin::Arguments* args) {
  auto* view = new View();
  view->InitWithArgs(args);
  return view;
}

// static
void View::BuildPrototype(v8::Isolate* isolate,
                          v8::Local<v8::FunctionTemplate> prototype) {
  prototype->SetClassName(gin::StringToV8(isolate, "View"));
#if BUILDFLAG(ENABLE_VIEWS_API)
  gin_helper::ObjectTemplateBuilder(isolate, prototype->PrototypeTemplate())
      .SetMethod("addChildView", &View::AddChildView)
      .SetMethod("addChildViewAt", &View::AddChildViewAt)
      .SetMethod("setStyle", &View::SetStyle);
#endif
}

}  // namespace api

}  // namespace electron

namespace {

using electron::api::View;

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  View::SetConstructor(isolate, base::BindRepeating(&View::New));

  gin_helper::Dictionary constructor(
      isolate,
      View::GetConstructor(isolate)->GetFunction(context).ToLocalChecked());

  gin_helper::Dictionary dict(isolate, exports);
  dict.Set("View", constructor);
}

}  // namespace

NODE_LINKED_MODULE_CONTEXT_AWARE(electron_browser_view, Initialize)
