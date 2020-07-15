// Copyright (c) 2020 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/api/views/electron_api_text_view.h"

#include "shell/common/gin_helper/constructor.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/node_includes.h"
#include "third_party/yoga/yoga/Yoga.h"

namespace electron {

namespace api {

namespace {

YGSize MeasureText(YGNodeRef node,
                   float width,
                   YGMeasureMode mode,
                   float height,
                   YGMeasureMode height_mode) {
  auto* text_view = static_cast<TextView*>(YGNodeGetContext(node));
  gfx::Size size = text_view->view()->GetPreferredSize();
  return {size.width(), size.height()};
}

}  // namespace

TextView::TextView(const base::string16& text)
    : View(new views::StyledLabel(text, this)) {
  YGNodeSetMeasureFunc(yoga_node(), MeasureText);
  YGNodeMarkDirty(yoga_node());
}

TextView::~TextView() {}

void TextView::SetText(const base::string16& text) {
  label()->SetText(text);
  YGNodeMarkDirty(yoga_node());
}

const base::string16& TextView::GetText() const {
  return label()->GetText();
}

// static
gin_helper::WrappableBase* TextView::New(const base::string16& text,
                                         gin_helper::Arguments* args) {
  // Constructor call.
  auto* view = new TextView(text);
  view->InitWithArgs(args);
  return view;
}

// static
void TextView::BuildPrototype(v8::Isolate* isolate,
                              v8::Local<v8::FunctionTemplate> prototype) {
  prototype->SetClassName(gin::StringToV8(isolate, "TextView"));
  gin_helper::ObjectTemplateBuilder(isolate, prototype->PrototypeTemplate())
      .SetMethod("setText", &TextView::SetText)
      .SetMethod("getText", &TextView::GetText);
}

}  // namespace api

}  // namespace electron

namespace {

using electron::api::TextView;

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  gin_helper::Dictionary dict(isolate, exports);
  dict.Set("TextView", gin_helper::CreateConstructor<TextView>(
                           isolate, base::BindRepeating(&TextView::New)));
}

}  // namespace

NODE_LINKED_MODULE_CONTEXT_AWARE(electron_browser_text_view, Initialize)
