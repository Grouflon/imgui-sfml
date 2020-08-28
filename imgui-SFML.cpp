#include "imgui-SFML.h"

#include <SFML/Config.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/Window/Clipboard.hpp>
#include <SFML/Window/Cursor.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Touch.hpp>
#include <SFML/Window/Window.hpp>

#include <cassert>
#include <cmath>    // abs
#include <cstddef>  // offsetof, NULL
#include <cstring>  // memcpy

#ifdef ANDROID
#ifdef USE_JNI

#include <android/native_activity.h>
#include <jni.h>
#include <SFML/System/NativeActivity.hpp>

static bool s_wantTextInput = false;

int openKeyboardIME() {
    ANativeActivity* activity = sf::getNativeActivity();
    JavaVM* vm = activity->vm;
    JNIEnv* env = activity->env;
    JavaVMAttachArgs attachargs;
    attachargs.version = JNI_VERSION_1_6;
    attachargs.name = "NativeThread";
    attachargs.group = NULL;
    jint res = vm->AttachCurrentThread(&env, &attachargs);
    if (res == JNI_ERR) return EXIT_FAILURE;

    jclass natact = env->FindClass("android/app/NativeActivity");
    jclass context = env->FindClass("android/content/Context");

    jfieldID fid = env->GetStaticFieldID(context, "INPUT_METHOD_SERVICE",
                                         "Ljava/lang/String;");
    jobject svcstr = env->GetStaticObjectField(context, fid);

    jmethodID getss = env->GetMethodID(
        natact, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jobject imm_obj = env->CallObjectMethod(activity->clazz, getss, svcstr);

    jclass imm_cls = env->GetObjectClass(imm_obj);
    jmethodID toggleSoftInput =
        env->GetMethodID(imm_cls, "toggleSoftInput", "(II)V");

    env->CallVoidMethod(imm_obj, toggleSoftInput, 2, 0);

    env->DeleteLocalRef(imm_obj);
    env->DeleteLocalRef(imm_cls);
    env->DeleteLocalRef(svcstr);
    env->DeleteLocalRef(context);
    env->DeleteLocalRef(natact);

    vm->DetachCurrentThread();

    return EXIT_SUCCESS;
}

int closeKeyboardIME() {
    ANativeActivity* activity = sf::getNativeActivity();
    JavaVM* vm = activity->vm;
    JNIEnv* env = activity->env;
    JavaVMAttachArgs attachargs;
    attachargs.version = JNI_VERSION_1_6;
    attachargs.name = "NativeThread";
    attachargs.group = NULL;
    jint res = vm->AttachCurrentThread(&env, &attachargs);
    if (res == JNI_ERR) return EXIT_FAILURE;

    jclass natact = env->FindClass("android/app/NativeActivity");
    jclass context = env->FindClass("android/content/Context");

    jfieldID fid = env->GetStaticFieldID(context, "INPUT_METHOD_SERVICE",
                                         "Ljava/lang/String;");
    jobject svcstr = env->GetStaticObjectField(context, fid);

    jmethodID getss = env->GetMethodID(
        natact, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jobject imm_obj = env->CallObjectMethod(activity->clazz, getss, svcstr);

    jclass imm_cls = env->GetObjectClass(imm_obj);
    jmethodID toggleSoftInput =
        env->GetMethodID(imm_cls, "toggleSoftInput", "(II)V");

    env->CallVoidMethod(imm_obj, toggleSoftInput, 1, 0);

    env->DeleteLocalRef(imm_obj);
    env->DeleteLocalRef(imm_cls);
    env->DeleteLocalRef(svcstr);
    env->DeleteLocalRef(context);
    env->DeleteLocalRef(natact);

    vm->DetachCurrentThread();

    return EXIT_SUCCESS;
}

#endif
#endif
#include <imgui_internal.h>

#if __cplusplus >= 201103L  // C++11 and above
static_assert(sizeof(GLuint) <= sizeof(ImTextureID),
              "ImTextureID is not large enough to fit GLuint.");
#endif

namespace {




// various helper functions
ImColor toImColor(sf::Color c);
ImVec2 getTopLeftAbsolute(const sf::FloatRect& rect);
ImVec2 getDownRightAbsolute(const sf::FloatRect& rect);

ImTextureID convertGLTextureHandleToImTextureID(GLuint glTextureHandle);
GLuint convertImTextureIDToGLTextureHandle(ImTextureID textureID);

void RenderDrawLists(
    ImDrawData* draw_data);  // rendering callback function prototype

// Implementation of ImageButton overload
bool imageButtonImpl(const sf::Texture& texture,
                     const sf::FloatRect& textureRect, const sf::Vector2f& size,
                     const int framePadding, const sf::Color& bgColor,
                     const sf::Color& tintColor);

// Default mapping is XInput gamepad mapping
void initDefaultJoystickMapping(ImGui::SFML::ImGuiSFMLContext& context);

// Returns first id of connected joystick
unsigned int getConnectedJoystickId();

void updateJoystickActionState(ImGui::SFML::ImGuiSFMLContext& context, ImGuiIO& io, ImGuiNavInput_ action);
void updateJoystickDPadState(ImGui::SFML::ImGuiSFMLContext& context, ImGuiIO& io);
void updateJoystickLStickState(ImGui::SFML::ImGuiSFMLContext& context, ImGuiIO& io);

// clipboard functions
void setClipboardText(void* userData, const char* text);
const char* getClipboadText(void* userData);

// mouse cursors
void loadMouseCursor(ImGui::SFML::ImGuiSFMLContext& context, ImGuiMouseCursor imguiCursorType,
                     sf::Cursor::Type sfmlCursorType);
void updateMouseCursor(ImGui::SFML::ImGuiSFMLContext& context, sf::Window& window);

}  // namespace

namespace ImGui {
namespace SFML {

const unsigned int NULL_JOYSTICK_ID = sf::Joystick::Count;
const unsigned int NULL_JOYSTICK_BUTTON = sf::Joystick::ButtonCount;

void Init(ImGuiSFMLContext& context, sf::RenderWindow& window, bool loadDefaultFont) {
    Init(context, window, window, loadDefaultFont);
}

void Init(ImGuiSFMLContext& context, sf::Window& window, sf::RenderTarget& target, bool loadDefaultFont) {
    Init(context, window, static_cast<sf::Vector2f>(target.getSize()), loadDefaultFont);
}

void Init(ImGuiSFMLContext& context, sf::Window& window, const sf::Vector2f& displaySize, bool loadDefaultFont) {
#if __cplusplus < 201103L  // runtime assert when using earlier than C++11 as no
                           // static_assert support
    assert(
        sizeof(GLuint) <=
        sizeof(ImTextureID));  // ImTextureID is not large enough to fit GLuint.
#endif

    context.imguiContext = ImGui::CreateContext();
    ImGuiIO& io = context.imguiContext->IO;

    // tell ImGui which features we support
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    io.BackendPlatformName = "imgui_impl_sfml";

    // init keyboard mapping
    io.KeyMap[ImGuiKey_Tab] = sf::Keyboard::Tab;
    io.KeyMap[ImGuiKey_LeftArrow] = sf::Keyboard::Left;
    io.KeyMap[ImGuiKey_RightArrow] = sf::Keyboard::Right;
    io.KeyMap[ImGuiKey_UpArrow] = sf::Keyboard::Up;
    io.KeyMap[ImGuiKey_DownArrow] = sf::Keyboard::Down;
    io.KeyMap[ImGuiKey_PageUp] = sf::Keyboard::PageUp;
    io.KeyMap[ImGuiKey_PageDown] = sf::Keyboard::PageDown;
    io.KeyMap[ImGuiKey_Home] = sf::Keyboard::Home;
    io.KeyMap[ImGuiKey_End] = sf::Keyboard::End;
    io.KeyMap[ImGuiKey_Insert] = sf::Keyboard::Insert;
#ifdef ANDROID
    io.KeyMap[ImGuiKey_Backspace] = sf::Keyboard::Delete;
#else
    io.KeyMap[ImGuiKey_Delete] = sf::Keyboard::Delete;
    io.KeyMap[ImGuiKey_Backspace] = sf::Keyboard::BackSpace;
#endif
    io.KeyMap[ImGuiKey_Space] = sf::Keyboard::Space;
    io.KeyMap[ImGuiKey_Enter] = sf::Keyboard::Return;
    io.KeyMap[ImGuiKey_Escape] = sf::Keyboard::Escape;
    io.KeyMap[ImGuiKey_A] = sf::Keyboard::A;
    io.KeyMap[ImGuiKey_C] = sf::Keyboard::C;
    io.KeyMap[ImGuiKey_V] = sf::Keyboard::V;
    io.KeyMap[ImGuiKey_X] = sf::Keyboard::X;
    io.KeyMap[ImGuiKey_Y] = sf::Keyboard::Y;
    io.KeyMap[ImGuiKey_Z] = sf::Keyboard::Z;

    context.joystickId = getConnectedJoystickId();

    for (unsigned int i = 0; i < ImGuiNavInput_COUNT; i++) {
        context.joystickMapping[i] = NULL_JOYSTICK_BUTTON;
    }

    initDefaultJoystickMapping(context);

    // init rendering
    io.DisplaySize = ImVec2(displaySize.x, displaySize.y);

    // clipboard
    io.SetClipboardTextFn = setClipboardText;
    io.GetClipboardTextFn = getClipboadText;
    io.ClipboardUserData = &context;

    // load mouse cursors
    for (int i = 0; i < ImGuiMouseCursor_COUNT; ++i) {
        context.mouseCursorLoaded[i] = false;
    }

    loadMouseCursor(context, ImGuiMouseCursor_Arrow, sf::Cursor::Arrow);
    loadMouseCursor(context, ImGuiMouseCursor_TextInput, sf::Cursor::Text);
    loadMouseCursor(context, ImGuiMouseCursor_ResizeAll, sf::Cursor::SizeAll);
    loadMouseCursor(context, ImGuiMouseCursor_ResizeNS, sf::Cursor::SizeVertical);
    loadMouseCursor(context, ImGuiMouseCursor_ResizeEW, sf::Cursor::SizeHorizontal);
    loadMouseCursor(context, ImGuiMouseCursor_ResizeNESW,
                    sf::Cursor::SizeBottomLeftTopRight);
    loadMouseCursor(context, ImGuiMouseCursor_ResizeNWSE,
                    sf::Cursor::SizeTopLeftBottomRight);
    loadMouseCursor(context, ImGuiMouseCursor_Hand, sf::Cursor::Hand);

    if (context.fontTexture) {  // delete previously created texture
        delete context.fontTexture;
    }
    context.fontTexture = new sf::Texture;

    if (loadDefaultFont) {
        // this will load default font automatically
        // No need to call AddDefaultFont
        UpdateFontTexture(context);
    }

    context.windowHasFocus = window.hasFocus();
}

void ProcessEvent(ImGuiSFMLContext& context, const sf::Event& event) {
    if (context.windowHasFocus) {
        ImGuiIO& io = context.imguiContext->IO;

        switch (event.type) {
            case sf::Event::MouseMoved:
                context.mouseMoved = true;
                break;
            case sf::Event::MouseButtonPressed:  // fall-through
            case sf::Event::MouseButtonReleased: {
                int button = event.mouseButton.button;
                if (event.type == sf::Event::MouseButtonPressed &&
                    button >= 0 && button < 3) {
                    context.mousePressed[event.mouseButton.button] = true;
                }
            } break;
            case sf::Event::TouchBegan:  // fall-through
            case sf::Event::TouchEnded: {
                context.mouseMoved = false;
                int button = event.touch.finger;
                if (event.type == sf::Event::TouchBegan && button >= 0 &&
                    button < 3) {
                    context.touchDown[event.touch.finger] = true;
                }
            } break;
            case sf::Event::MouseWheelScrolled:
                if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel ||
                    (event.mouseWheelScroll.wheel == sf::Mouse::HorizontalWheel &&
                    io.KeyShift)) {
                    io.MouseWheel += event.mouseWheelScroll.delta;
                } else if (event.mouseWheelScroll.wheel == sf::Mouse::HorizontalWheel) {
                    io.MouseWheelH += event.mouseWheelScroll.delta;
                }
                break;
            case sf::Event::KeyPressed:  // fall-through
            case sf::Event::KeyReleased:
                io.KeysDown[event.key.code] =
                    (event.type == sf::Event::KeyPressed);
                break;
            case sf::Event::TextEntered:
                // Don't handle the event for unprintable characters
                if (event.text.unicode < ' ' || event.text.unicode == 127) {
                    break;
                }
                io.AddInputCharacter(event.text.unicode);
                break;
            case sf::Event::JoystickConnected:
                if (context.joystickId == NULL_JOYSTICK_ID) {
                    context.joystickId = event.joystickConnect.joystickId;
                }
                break;
            case sf::Event::JoystickDisconnected:
                if (context.joystickId ==
                    event.joystickConnect
                        .joystickId) {  // used gamepad was disconnected
                    context.joystickId = getConnectedJoystickId();
                }
                break;
            default:
                break;
        }
    }

    switch (event.type) {
        case sf::Event::LostFocus:
            context.windowHasFocus = false;
            break;
        case sf::Event::GainedFocus:
            context.windowHasFocus = true;
            break;
        default:
            break;
    }
}

void Update(ImGuiSFMLContext& context, sf::RenderWindow& window, sf::Time dt) {
    Update(context, window, window, dt);
}

void Update(ImGuiSFMLContext& context, sf::Window& window, sf::RenderTarget& target, sf::Time dt) {
	ImGuiIO& io = context.imguiContext->IO;
    // Update OS/hardware mouse cursor if imgui isn't drawing a software cursor
    updateMouseCursor(context, window);

    if (!context.mouseMoved) {
        if (sf::Touch::isDown(0))
            context.touchPos = sf::Touch::getPosition(0, window);

        Update(context, context.touchPos, static_cast<sf::Vector2f>(target.getSize()), dt);
    } else {
        Update(context, sf::Mouse::getPosition(window),
               static_cast<sf::Vector2f>(target.getSize()), dt);
    }

    if (io.MouseDrawCursor) {
        // Hide OS mouse cursor if imgui is drawing it
        window.setMouseCursorVisible(false);
    }
}

void Update(ImGuiSFMLContext& context, const sf::Vector2i& mousePos, const sf::Vector2f& displaySize,
            sf::Time dt) {
	ImGuiIO& io = context.imguiContext->IO;
    io.DisplaySize = ImVec2(displaySize.x, displaySize.y);
    
    io.DeltaTime = dt.asSeconds();

    if (context.windowHasFocus) {
        if (io.WantSetMousePos) {
            sf::Vector2i mousePos(static_cast<int>(io.MousePos.x),
                                  static_cast<int>(io.MousePos.y));
            sf::Mouse::setPosition(mousePos);
        } else {
            io.MousePos = ImVec2(mousePos.x, mousePos.y);
        }
        for (unsigned int i = 0; i < 3; i++) {
            io.MouseDown[i] = context.touchDown[i] || sf::Touch::isDown(i) ||
                              context.mousePressed[i] ||
                              sf::Mouse::isButtonPressed((sf::Mouse::Button)i);
            context.mousePressed[i] = false;
            context.touchDown[i] = false;
        }
    }

    // Update Ctrl, Shift, Alt, Super state
    io.KeyCtrl = io.KeysDown[sf::Keyboard::LControl] ||
                 io.KeysDown[sf::Keyboard::RControl];
    io.KeyAlt =
        io.KeysDown[sf::Keyboard::LAlt] || io.KeysDown[sf::Keyboard::RAlt];
    io.KeyShift =
        io.KeysDown[sf::Keyboard::LShift] || io.KeysDown[sf::Keyboard::RShift];
    io.KeySuper = io.KeysDown[sf::Keyboard::LSystem] ||
                  io.KeysDown[sf::Keyboard::RSystem];

#ifdef ANDROID
#ifdef USE_JNI
    if (io.WantTextInput && !s_wantTextInput) {
        openKeyboardIME();
        s_wantTextInput = true;
    }

    if (!io.WantTextInput && s_wantTextInput) {
        closeKeyboardIME();
        s_wantTextInput = false;
    }
#endif
#endif

    assert(io.Fonts->Fonts.Size > 0);  // You forgot to create and set up font
                                       // atlas (see createFontTexture)

    // gamepad navigation
    if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) &&
        context.joystickId != NULL_JOYSTICK_ID) {
        updateJoystickActionState(context, io, ImGuiNavInput_Activate);
        updateJoystickActionState(context, io, ImGuiNavInput_Cancel);
        updateJoystickActionState(context, io, ImGuiNavInput_Input);
        updateJoystickActionState(context, io, ImGuiNavInput_Menu);

        updateJoystickActionState(context, io, ImGuiNavInput_FocusPrev);
        updateJoystickActionState(context, io, ImGuiNavInput_FocusNext);

        updateJoystickActionState(context, io, ImGuiNavInput_TweakSlow);
        updateJoystickActionState(context, io, ImGuiNavInput_TweakFast);

        updateJoystickDPadState(context, io);
        updateJoystickLStickState(context, io);
    }

    ImGui::SetCurrentContext(context.imguiContext);
    ImGui::NewFrame();
}

void Render(ImGuiSFMLContext& context, sf::RenderTarget& target) {
    target.resetGLStates();
    Render(context);
}

void Render(ImGuiSFMLContext& context) {
	ImGui::SetCurrentContext(context.imguiContext);
    ImGui::Render();
    RenderDrawLists(ImGui::GetDrawData());
}

void Shutdown(ImGuiSFMLContext& context) {
	ImGuiIO& io = context.imguiContext->IO;
    io.Fonts->TexID = (ImTextureID)NULL;

    if (context.fontTexture) {  // if internal texture was created, we delete it
        delete context.fontTexture;
        context.fontTexture = NULL;
    }

    for (int i = 0; i < ImGuiMouseCursor_COUNT; ++i) {
        if (context.mouseCursorLoaded[i]) {
            delete context.mouseCursors[i];
            context.mouseCursors[i] = NULL;

            context.mouseCursorLoaded[i] = false;
        }
    }

	ImGui::SetCurrentContext(context.imguiContext);
    ImGui::DestroyContext();
    context.imguiContext = NULL;
}

void UpdateFontTexture(ImGuiSFMLContext& context) {
	ImGuiIO& io = context.imguiContext->IO;
    unsigned char* pixels;
    int width, height;

    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    sf::Texture& texture = *context.fontTexture;
    texture.create(width, height);
    texture.update(pixels);

    io.Fonts->TexID =
        convertGLTextureHandleToImTextureID(texture.getNativeHandle());
}

sf::Texture& GetFontTexture(ImGuiSFMLContext& context) { return *context.fontTexture; }

void SetActiveJoystickId(ImGuiSFMLContext& context, unsigned int joystickId) {
    assert(joystickId < sf::Joystick::Count);
    context.joystickId = joystickId;
}

void SetJoytickDPadThreshold(ImGuiSFMLContext& context, float threshold) {
    assert(threshold >= 0.f && threshold <= 100.f);
    context.dPadInfo.threshold = threshold;
}

void SetJoytickLStickThreshold(ImGuiSFMLContext& context, float threshold) {
    assert(threshold >= 0.f && threshold <= 100.f);
    context.lStickInfo.threshold = threshold;
}

void SetJoystickMapping(ImGuiSFMLContext& context, int action, unsigned int joystickButton) {
    assert(action < ImGuiNavInput_COUNT);
    assert(joystickButton < sf::Joystick::ButtonCount);
    context.joystickMapping[action] = joystickButton;
}

void SetDPadXAxis(ImGuiSFMLContext& context, sf::Joystick::Axis dPadXAxis, bool inverted) {
    context.dPadInfo.xAxis = dPadXAxis;
    context.dPadInfo.xInverted = inverted;
}

void SetDPadYAxis(ImGuiSFMLContext& context, sf::Joystick::Axis dPadYAxis, bool inverted) {
    context.dPadInfo.yAxis = dPadYAxis;
    context.dPadInfo.yInverted = inverted;
}

void SetLStickXAxis(ImGuiSFMLContext& context, sf::Joystick::Axis lStickXAxis, bool inverted) {
    context.lStickInfo.xAxis = lStickXAxis;
    context.lStickInfo.xInverted = inverted;
}

void SetLStickYAxis(ImGuiSFMLContext& context, sf::Joystick::Axis lStickYAxis, bool inverted) {
    context.lStickInfo.yAxis = lStickYAxis;
    context.lStickInfo.yInverted = inverted;
}

}  // end of namespace SFML

/////////////// Image Overloads

void Image(const sf::Texture& texture, const sf::Color& tintColor,
           const sf::Color& borderColor) {
    Image(texture, static_cast<sf::Vector2f>(texture.getSize()), tintColor,
          borderColor);
}

void Image(const sf::Texture& texture, const sf::Vector2f& size,
           const sf::Color& tintColor, const sf::Color& borderColor) {
    ImTextureID textureID =
        convertGLTextureHandleToImTextureID(texture.getNativeHandle());
    
    ImGui::Image(textureID, ImVec2(size.x,size.y), ImVec2(0, 0), ImVec2(1, 1), toImColor(tintColor),
        toImColor(borderColor));
}

void Image(const sf::Texture& texture, const sf::FloatRect& textureRect,
           const sf::Color& tintColor, const sf::Color& borderColor) {
    Image(
        texture,
        sf::Vector2f(std::abs(textureRect.width), std::abs(textureRect.height)),
        textureRect, tintColor, borderColor);
}

void Image(const sf::Texture& texture, const sf::Vector2f& size,
           const sf::FloatRect& textureRect, const sf::Color& tintColor,
           const sf::Color& borderColor) {
    sf::Vector2f textureSize = static_cast<sf::Vector2f>(texture.getSize());
    ImVec2 uv0(textureRect.left / textureSize.x,
               textureRect.top / textureSize.y);
    ImVec2 uv1((textureRect.left + textureRect.width) / textureSize.x,
               (textureRect.top + textureRect.height) / textureSize.y);

    ImTextureID textureID =
        convertGLTextureHandleToImTextureID(texture.getNativeHandle());
    ImGui::Image(textureID, ImVec2(size.x, size.y), uv0, uv1, toImColor(tintColor),
        toImColor(borderColor));
}

void Image(const sf::Sprite& sprite, const sf::Color& tintColor,
           const sf::Color& borderColor) {
    sf::FloatRect bounds = sprite.getGlobalBounds();
    Image(sprite, sf::Vector2f(bounds.width, bounds.height), tintColor,
          borderColor);
}

void Image(const sf::Sprite& sprite, const sf::Vector2f& size,
           const sf::Color& tintColor, const sf::Color& borderColor) {
    const sf::Texture* texturePtr = sprite.getTexture();
    // sprite without texture cannot be drawn
    if (!texturePtr) {
        return;
    }

    Image(*texturePtr, size,
          static_cast<sf::FloatRect>(sprite.getTextureRect()), tintColor,
          borderColor);
}

/////////////// Image Button Overloads

bool ImageButton(const sf::Texture& texture, const int framePadding,
                 const sf::Color& bgColor, const sf::Color& tintColor) {
    return ImageButton(texture, static_cast<sf::Vector2f>(texture.getSize()),
                       framePadding, bgColor, tintColor);
}

bool ImageButton(const sf::Texture& texture, const sf::Vector2f& size,
                 const int framePadding, const sf::Color& bgColor,
                 const sf::Color& tintColor) {
    sf::Vector2f textureSize = static_cast<sf::Vector2f>(texture.getSize());
    return ::imageButtonImpl(
        texture, sf::FloatRect(0.f, 0.f, textureSize.x, textureSize.y), size,
        framePadding, bgColor, tintColor);
}

bool ImageButton(const sf::Sprite& sprite, const int framePadding,
                 const sf::Color& bgColor, const sf::Color& tintColor) {
    sf::FloatRect spriteSize = sprite.getGlobalBounds();
    return ImageButton(sprite,
                       sf::Vector2f(spriteSize.width, spriteSize.height),
                       framePadding, bgColor, tintColor);
}

bool ImageButton(const sf::Sprite& sprite, const sf::Vector2f& size,
                 const int framePadding, const sf::Color& bgColor,
                 const sf::Color& tintColor) {
    const sf::Texture* texturePtr = sprite.getTexture();
    if (!texturePtr) {
        return false;
    }
    return ::imageButtonImpl(
        *texturePtr, static_cast<sf::FloatRect>(sprite.getTextureRect()), size,
        framePadding, bgColor, tintColor);
}

/////////////// Draw_list Overloads

void DrawLine(const sf::Vector2f& a, const sf::Vector2f& b,
              const sf::Color& color, float thickness) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    draw_list->AddLine(ImVec2(a.x + pos.x, a.y + pos.y), ImVec2(b.x + pos.x, b.y + pos.y), ColorConvertFloat4ToU32(toImColor(color)),
                       thickness);
}

void DrawRect(const sf::FloatRect& rect, const sf::Color& color, float rounding,
              int rounding_corners, float thickness) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRect(getTopLeftAbsolute(rect), getDownRightAbsolute(rect),
                       ColorConvertFloat4ToU32(toImColor(color)), rounding,
                       rounding_corners, thickness);
}

void DrawRectFilled(const sf::FloatRect& rect, const sf::Color& color,
                    float rounding, int rounding_corners) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(
        getTopLeftAbsolute(rect), getDownRightAbsolute(rect),
        ColorConvertFloat4ToU32(toImColor(color)), rounding, rounding_corners);
}

}  // end of namespace ImGui

namespace {
ImColor toImColor(sf::Color c ) {
    return ImColor(static_cast<int>(c.r), static_cast<int>(c.g), static_cast<int>(c.b), static_cast<int>(c.a));
}
ImVec2 getTopLeftAbsolute(const sf::FloatRect& rect) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    return ImVec2(rect.left + pos.x, rect.top + pos.y);
}
ImVec2 getDownRightAbsolute(const sf::FloatRect& rect) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    return ImVec2(rect.left + rect.width + pos.x,
                  rect.top + rect.height + pos.y);
}

ImTextureID convertGLTextureHandleToImTextureID(GLuint glTextureHandle) {
    ImTextureID textureID = (ImTextureID)NULL;
    std::memcpy(&textureID, &glTextureHandle, sizeof(GLuint));
    return textureID;
}
GLuint convertImTextureIDToGLTextureHandle(ImTextureID textureID) {
    GLuint glTextureHandle;
    std::memcpy(&glTextureHandle, &textureID, sizeof(GLuint));
    return glTextureHandle;
}

// Rendering callback
void RenderDrawLists(ImDrawData* draw_data) {
    ImGui::GetDrawData();
    if (draw_data->CmdListsCount == 0) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    assert(io.Fonts->TexID !=
           (ImTextureID)NULL);  // You forgot to create and set font texture

    // scale stuff (needed for proper handling of window resize)
    int fb_width =
        static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fb_height =
        static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fb_width == 0 || fb_height == 0) {
        return;
    }
    draw_data->ScaleClipRects(io.DisplayFramebufferScale);

#ifdef GL_VERSION_ES_CL_1_1
    GLint last_program, last_texture, last_array_buffer,
        last_element_array_buffer;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
#else
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT);
#endif

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

#ifdef GL_VERSION_ES_CL_1_1
    glOrthof(0.0f, io.DisplaySize.x, io.DisplaySize.y, 0.0f, -1.0f, +1.0f);
#else
    glOrtho(0.0f, io.DisplaySize.x, io.DisplaySize.y, 0.0f, -1.0f, +1.0f);
#endif

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    for (int n = 0; n < draw_data->CmdListsCount; ++n) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const unsigned char* vtx_buffer =
            (const unsigned char*)&cmd_list->VtxBuffer.front();
        const ImDrawIdx* idx_buffer = &cmd_list->IdxBuffer.front();

        glVertexPointer(2, GL_FLOAT, sizeof(ImDrawVert),
                        (void*)(vtx_buffer + offsetof(ImDrawVert, pos)));
        glTexCoordPointer(2, GL_FLOAT, sizeof(ImDrawVert),
                          (void*)(vtx_buffer + offsetof(ImDrawVert, uv)));
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ImDrawVert),
                       (void*)(vtx_buffer + offsetof(ImDrawVert, col)));

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); ++cmd_i) {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback) {
                pcmd->UserCallback(cmd_list, pcmd);
            } else {
                GLuint textureHandle =
                    convertImTextureIDToGLTextureHandle(pcmd->TextureId);
                glBindTexture(GL_TEXTURE_2D, textureHandle);
                glScissor((int)pcmd->ClipRect.x,
                          (int)(fb_height - pcmd->ClipRect.w),
                          (int)(pcmd->ClipRect.z - pcmd->ClipRect.x),
                          (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
                glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount,
                               GL_UNSIGNED_SHORT, idx_buffer);
            }
            idx_buffer += pcmd->ElemCount;
        }
    }
#ifdef GL_VERSION_ES_CL_1_1
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
    glDisable(GL_SCISSOR_TEST);
#else
    glPopAttrib();
#endif
}

bool imageButtonImpl(const sf::Texture& texture,
                     const sf::FloatRect& textureRect, const sf::Vector2f& size,
                     const int framePadding, const sf::Color& bgColor,
                     const sf::Color& tintColor) {
    sf::Vector2f textureSize = static_cast<sf::Vector2f>(texture.getSize());

    ImVec2 uv0(textureRect.left / textureSize.x,
               textureRect.top / textureSize.y);
    ImVec2 uv1((textureRect.left + textureRect.width) / textureSize.x,
               (textureRect.top + textureRect.height) / textureSize.y);

    ImTextureID textureID =
        convertGLTextureHandleToImTextureID(texture.getNativeHandle());
    return ImGui::ImageButton(textureID, ImVec2(size.x,size.y), uv0, uv1, framePadding, toImColor(bgColor),
        toImColor(tintColor));
}

unsigned int getConnectedJoystickId() {
    for (unsigned int i = 0; i < (unsigned int)sf::Joystick::Count; ++i) {
        if (sf::Joystick::isConnected(i)) return i;
    }

    return ImGui::SFML::NULL_JOYSTICK_ID;
}

void initDefaultJoystickMapping(ImGui::SFML::ImGuiSFMLContext& context) {
    ImGui::SFML::SetJoystickMapping(context, ImGuiNavInput_Activate, 0);
    ImGui::SFML::SetJoystickMapping(context, ImGuiNavInput_Cancel, 1);
    ImGui::SFML::SetJoystickMapping(context, ImGuiNavInput_Input, 3);
    ImGui::SFML::SetJoystickMapping(context, ImGuiNavInput_Menu, 2);
    ImGui::SFML::SetJoystickMapping(context, ImGuiNavInput_FocusPrev, 4);
    ImGui::SFML::SetJoystickMapping(context, ImGuiNavInput_FocusNext, 5);
    ImGui::SFML::SetJoystickMapping(context, ImGuiNavInput_TweakSlow, 4);
    ImGui::SFML::SetJoystickMapping(context, ImGuiNavInput_TweakFast, 5);

    ImGui::SFML::SetDPadXAxis(context, sf::Joystick::PovX);
    // D-pad Y axis is inverted on Windows
#ifdef _WIN32
    ImGui::SFML::SetDPadYAxis(context, sf::Joystick::PovY, true);
#else
    ImGui::SFML::SetDPadYAxis(sf::Joystick::PovY);
#endif

    ImGui::SFML::SetLStickXAxis(context, sf::Joystick::X);
    ImGui::SFML::SetLStickYAxis(context, sf::Joystick::Y);

    ImGui::SFML::SetJoytickDPadThreshold(context, 5.f);
    ImGui::SFML::SetJoytickLStickThreshold(context, 5.f);
}

void updateJoystickActionState(ImGui::SFML::ImGuiSFMLContext& context, ImGuiIO& io, ImGuiNavInput_ action) {
    bool isPressed =
        sf::Joystick::isButtonPressed(context.joystickId, context.joystickMapping[action]);
    io.NavInputs[action] = isPressed ? 1.0f : 0.0f;
}

void updateJoystickDPadState(ImGui::SFML::ImGuiSFMLContext& context, ImGuiIO& io) {
    float dpadXPos =
        sf::Joystick::getAxisPosition(context.joystickId, context.dPadInfo.xAxis);
    if (context.dPadInfo.xInverted) dpadXPos = -dpadXPos;

    float dpadYPos =
        sf::Joystick::getAxisPosition(context.joystickId, context.dPadInfo.yAxis);
    if (context.dPadInfo.yInverted) dpadYPos = -dpadYPos;

    io.NavInputs[ImGuiNavInput_DpadLeft] =
        dpadXPos < -context.dPadInfo.threshold ? 1.0f : 0.0f;
    io.NavInputs[ImGuiNavInput_DpadRight] =
        dpadXPos > context.dPadInfo.threshold ? 1.0f : 0.0f;

    io.NavInputs[ImGuiNavInput_DpadUp] =
        dpadYPos < -context.dPadInfo.threshold ? 1.0f : 0.0f;
    io.NavInputs[ImGuiNavInput_DpadDown] =
        dpadYPos > context.dPadInfo.threshold ? 1.0f : 0.0f;
}

void updateJoystickLStickState(ImGui::SFML::ImGuiSFMLContext& context, ImGuiIO& io) {
    float lStickXPos =
        sf::Joystick::getAxisPosition(context.joystickId, context.lStickInfo.xAxis);
    if (context.lStickInfo.xInverted) lStickXPos = -lStickXPos;

    float lStickYPos =
        sf::Joystick::getAxisPosition(context.joystickId, context.lStickInfo.yAxis);
    if (context.lStickInfo.yInverted) lStickYPos = -lStickYPos;

    if (lStickXPos < -context.lStickInfo.threshold) {
        io.NavInputs[ImGuiNavInput_LStickLeft] = std::abs(lStickXPos / 100.f);
    }

    if (lStickXPos > context.lStickInfo.threshold) {
        io.NavInputs[ImGuiNavInput_LStickRight] = lStickXPos / 100.f;
    }

    if (lStickYPos < -context.lStickInfo.threshold) {
        io.NavInputs[ImGuiNavInput_LStickUp] = std::abs(lStickYPos / 100.f);
    }

    if (lStickYPos > context.lStickInfo.threshold) {
        io.NavInputs[ImGuiNavInput_LStickDown] = lStickYPos / 100.f;
    }
}

void setClipboardText(void* userData, const char* text) {
    sf::Clipboard::setString(sf::String::fromUtf8(text, text + std::strlen(text)));
}

const char* getClipboadText(void* userData) {
    ImGui::SFML::ImGuiSFMLContext* contextPtr = (ImGui::SFML::ImGuiSFMLContext*)userData;
    std::basic_string<sf::Uint8> tmp = sf::Clipboard::getString().toUtf8();
    contextPtr->clipboardText = std::string(tmp.begin(), tmp.end());
    return contextPtr->clipboardText.c_str();
}

void loadMouseCursor(ImGui::SFML::ImGuiSFMLContext& context, ImGuiMouseCursor imguiCursorType,
                     sf::Cursor::Type sfmlCursorType) {
    context.mouseCursors[imguiCursorType] = new sf::Cursor();
    context.mouseCursorLoaded[imguiCursorType] =
        context.mouseCursors[imguiCursorType]->loadFromSystem(sfmlCursorType);
}

void updateMouseCursor(ImGui::SFML::ImGuiSFMLContext& context, sf::Window& window) {
	ImGuiIO& io = context.imguiContext->IO;
    if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) == 0) {
        ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
        if (io.MouseDrawCursor || cursor == ImGuiMouseCursor_None) {
            window.setMouseCursorVisible(false);
        } else {
            window.setMouseCursorVisible(true);

            sf::Cursor& c = context.mouseCursorLoaded[cursor]
                                ? *context.mouseCursors[cursor]
                                : *context.mouseCursors[ImGuiMouseCursor_Arrow];
            window.setMouseCursor(c);
        }
    }
}

}  // end of anonymous namespace
