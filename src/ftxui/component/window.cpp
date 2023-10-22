// Copyright 2023 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#define NOMINMAX
#include <algorithm>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>  // for ScreenInteractive
#include "ftxui/dom/node_decorator.hpp"            // for NodeDecorator

namespace ftxui {

namespace {

Decorator PositionAndSize(int left, int top, int width, int height) {
  return [=](Element element) {
    element |= size(WIDTH, EQUAL, width);
    element |= size(HEIGHT, EQUAL, height);

    auto padding_left = emptyElement() | size(WIDTH, EQUAL, left);
    auto padding_top = emptyElement() | size(HEIGHT, EQUAL, top);

    return vbox({
        padding_top,
        hbox({
            padding_left,
            element,
        }),
    });
  };
}

class DropShadowDecorator : public NodeDecorator {
public:
    DropShadowDecorator(Element child)
            : NodeDecorator(std::move(child)) {}

    void Render(Screen& screen) override {
        NodeDecorator::Render(screen);

        // right side of window
        for (int y = box_.y_min; y <= box_.y_max; ++y) {
            auto &cell = screen.PixelAt(box_.x_max+1, y+1);
            cell.foreground_color = Color::GrayDark;
            cell.background_color = Color::Black;
            cell.automerge = false;
        }

        // bottom side of window
        // stop short by 1 since the right side already handled the bottom right corner shadow
        for (int x = box_.x_min; x < box_.x_max; ++x) {
            auto& cell = screen.PixelAt(x+1, box_.y_max+1);
            cell.foreground_color = Color::GrayDark;
            cell.background_color = Color::Black;
            cell.automerge = false;
        }
    }
};


Element DefaultRenderState(const WindowRenderState& state) {
  Element element = state.inner;
  Element titleElement = center(text(state.title));
  if (!state.active) {
    element |= dim;
    titleElement |= dim;
  }

  element = window(titleElement, element, state.active);

  element |= bgcolor(Color::Cyan);
  element |= clear_under;

  element = std::make_shared<DropShadowDecorator>(element);

  return element;
}

class WindowImpl : public ComponentBase, public WindowOptions {
 public:
  explicit WindowImpl(WindowOptions option) : WindowOptions(std::move(option)) {
    if (!inner) {
      inner = Make<ComponentBase>();
    }
    Add(inner);
  }

 private:
  Element Render() final {
    auto element = ComponentBase::Render();

    const bool captureable =
        captured_mouse_ || ScreenInteractive::Active()->CaptureMouse();

    const WindowRenderState state = {
        element,
        title(),
        Active(),
        drag_,
        resize_bottom_right_,
    };

    element = render ? render(state) : DefaultRenderState(state);

    // Position and record the drawn area of the window.
    element |= reflect(box_window_);
    element |= PositionAndSize(left(), top(), width(), height());
    element |= reflect(box_);

    return element;
  }

  bool OnEvent(Event event) final {
    if (ComponentBase::OnEvent(event)) {
      return true;
    }

    if (!event.is_mouse()) {
      return false;
    }

    mouse_hover_ = box_window_.Contain(event.mouse().x, event.mouse().y);

    resize_bottom_right_hover_ = false;

    if (mouse_hover_) {
        resize_bottom_right_hover_ = event.mouse().x == box_window_.x_max && event.mouse().y == box_window_.y_max;

      // Apply the component options:
        resize_bottom_right_hover_ &= resize();
    }

    if (captured_mouse_) {
      if (event.mouse().motion == Mouse::Released) {
        captured_mouse_ = nullptr;
        return true;
      }

      if (resize_bottom_right_) {
        width() = event.mouse().x - resize_start_x - box_.x_min;
        height() = event.mouse().y - resize_start_y - box_.y_min;
      }

      if (drag_) {
        left() = event.mouse().x - drag_start_x - box_.x_min;
        top() = event.mouse().y - drag_start_y - box_.y_min;
      }

      // Clamp the window size.
      width() = std::max<int>(width(), title().size() + 2);
      height() = std::max<int>(height(), 2);

      return true;
    }

    resize_bottom_right_ = false;

    if (!mouse_hover_) {
      return false;
    }

    if (!CaptureMouse(event)) {
      return true;
    }

    if (event.mouse().button != Mouse::Left ||
        event.mouse().motion != Mouse::Pressed) {
      return true;
    }

    TakeFocus();

    captured_mouse_ = CaptureMouse(event);
    if (!captured_mouse_) {
      return true;
    }

    resize_bottom_right_ = resize_bottom_right_hover_;

    resize_start_x = event.mouse().x - width() - box_.x_min;
    resize_start_y = event.mouse().y - height() - box_.y_min;
    drag_start_x = event.mouse().x - left() - box_.x_min;
    drag_start_y = event.mouse().y - top() - box_.y_min;

    // Drag only if we are not resizeing a border yet:
    drag_ = !resize_bottom_right_;
    return true;
  }

  Box box_;
  Box box_window_;

  CapturedMouse captured_mouse_;
  int drag_start_x = 0;
  int drag_start_y = 0;
  int resize_start_x = 0;
  int resize_start_y = 0;

  bool mouse_hover_ = false;
  bool drag_ = false;
  bool resize_bottom_right_ = false;
  bool resize_bottom_right_hover_ = false;
};

}  // namespace

/// @brief A draggeable / resizeable window. To use multiple of them, they must
/// be stacked using `Container::Stacked({...})` component;
///
/// @param option A struct holding every parameters.
/// @ingroup component
/// @see Window
///
/// ### Example
///
/// ```cpp
/// auto window_1= Window({
///     .inner = DummyWindowContent(),
///     .title = "First window",
/// });
///
/// auto window_2= Window({
///     .inner = DummyWindowContent(),
///     .title = "Second window",
/// });
///
/// auto container = Container::Stacked({
///   window_1,
///   window_2,
/// });
/// ```
Component Window(WindowOptions option) {
  return Make<WindowImpl>(std::move(option));
}

};  // namespace ftxui
