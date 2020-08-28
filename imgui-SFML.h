#ifndef IMGUI_SFML_H
#define IMGUI_SFML_H

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/Window/Joystick.hpp>
#include <imgui.h>

#include "imgui-SFML_export.h"

namespace sf
{
    class Event;
    class RenderTarget;
    class RenderWindow;
    class Sprite;
    class Texture;
    class Window;
    class Cursor;
}

namespace ImGui
{
    namespace SFML
    {
        IMGUI_SFML_API extern const unsigned int NULL_JOYSTICK_ID;
        IMGUI_SFML_API extern const unsigned int NULL_JOYSTICK_BUTTON;

        IMGUI_SFML_API struct ImGuiSFMLContext
        {
			bool windowHasFocus = false;
			bool mousePressed[3] = { false, false, false };
			bool touchDown[3] = { false, false, false };
			bool mouseMoved = false;
			sf::Vector2i touchPos;
			sf::Texture* fontTexture = NULL; // owning pointer to internal font atlas which is used if user doesn't set custom sf::Texture.
			unsigned int joystickId = NULL_JOYSTICK_ID;
			unsigned int joystickMapping[ImGuiNavInput_COUNT];

			struct StickInfo {
				sf::Joystick::Axis xAxis;
				sf::Joystick::Axis yAxis;

				bool xInverted;
				bool yInverted;

				float threshold;
			};
			StickInfo dPadInfo;
			StickInfo lStickInfo;
			std::string clipboardText;
			sf::Cursor* mouseCursors[ImGuiMouseCursor_COUNT];
			bool mouseCursorLoaded[ImGuiMouseCursor_COUNT];
            ImGuiContext* imguiContext = NULL;
        };

        IMGUI_SFML_API void Init(ImGuiSFMLContext& context, sf::RenderWindow& window, bool loadDefaultFont = true);
        IMGUI_SFML_API void Init(ImGuiSFMLContext& context, sf::Window& window, sf::RenderTarget& target, bool loadDefaultFont = true);
        IMGUI_SFML_API void Init(ImGuiSFMLContext& context, sf::Window& window, const sf::Vector2f& displaySize, bool loadDefaultFont = true);

        IMGUI_SFML_API void ProcessEvent(ImGuiSFMLContext& context, const sf::Event& event);

        IMGUI_SFML_API void Update(ImGuiSFMLContext& context, sf::RenderWindow& window, sf::Time dt);
        IMGUI_SFML_API void Update(ImGuiSFMLContext& context, sf::Window& window, sf::RenderTarget& target, sf::Time dt);
        IMGUI_SFML_API void Update(ImGuiSFMLContext& context, const sf::Vector2i& mousePos, const sf::Vector2f& displaySize, sf::Time dt);

        IMGUI_SFML_API void Render(ImGuiSFMLContext& context, sf::RenderTarget& target);
        IMGUI_SFML_API void Render(ImGuiSFMLContext& context);

        IMGUI_SFML_API void Shutdown(ImGuiSFMLContext& context);

        IMGUI_SFML_API void UpdateFontTexture(ImGuiSFMLContext& context);
        IMGUI_SFML_API sf::Texture& GetFontTexture(ImGuiSFMLContext& context);

        // joystick functions
        IMGUI_SFML_API void SetActiveJoystickId(ImGuiSFMLContext& context, unsigned int joystickId);
        IMGUI_SFML_API void SetJoytickDPadThreshold(ImGuiSFMLContext& context, float threshold);
        IMGUI_SFML_API void SetJoytickLStickThreshold(ImGuiSFMLContext& context, float threshold);

        IMGUI_SFML_API void SetJoystickMapping(ImGuiSFMLContext& context, int action, unsigned int joystickButton);
        IMGUI_SFML_API void SetDPadXAxis(ImGuiSFMLContext& context, sf::Joystick::Axis dPadXAxis, bool inverted = false);
        IMGUI_SFML_API void SetDPadYAxis(ImGuiSFMLContext& context, sf::Joystick::Axis dPadYAxis, bool inverted = false);
        IMGUI_SFML_API void SetLStickXAxis(ImGuiSFMLContext& context, sf::Joystick::Axis lStickXAxis, bool inverted = false);
        IMGUI_SFML_API void SetLStickYAxis(ImGuiSFMLContext& context, sf::Joystick::Axis lStickYAxis, bool inverted = false);
    }

    // custom ImGui widgets for SFML stuff

    // Image overloads
    IMGUI_SFML_API void Image(const sf::Texture& texture,
        const sf::Color& tintColor = sf::Color::White,
        const sf::Color& borderColor = sf::Color::Transparent);
    IMGUI_SFML_API void Image(const sf::Texture& texture, const sf::Vector2f& size,
        const sf::Color& tintColor = sf::Color::White,
        const sf::Color& borderColor = sf::Color::Transparent);
    IMGUI_SFML_API void Image(const sf::Texture& texture, const sf::FloatRect& textureRect,
        const sf::Color& tintColor = sf::Color::White,
        const sf::Color& borderColor = sf::Color::Transparent);
    IMGUI_SFML_API void Image(const sf::Texture& texture, const sf::Vector2f& size, const sf::FloatRect& textureRect,
        const sf::Color& tintColor = sf::Color::White,
        const sf::Color& borderColor = sf::Color::Transparent);

    IMGUI_SFML_API void Image(const sf::Sprite& sprite,
        const sf::Color& tintColor = sf::Color::White,
        const sf::Color& borderColor = sf::Color::Transparent);
    IMGUI_SFML_API void Image(const sf::Sprite& sprite, const sf::Vector2f& size,
        const sf::Color& tintColor = sf::Color::White,
        const sf::Color& borderColor = sf::Color::Transparent);

    // ImageButton overloads
    IMGUI_SFML_API bool ImageButton(const sf::Texture& texture, const int framePadding = -1,
        const sf::Color& bgColor = sf::Color::Transparent,
        const sf::Color& tintColor = sf::Color::White);
    IMGUI_SFML_API bool ImageButton(const sf::Texture& texture, const sf::Vector2f& size, const int framePadding = -1,
        const sf::Color& bgColor = sf::Color::Transparent, const sf::Color& tintColor = sf::Color::White);

    IMGUI_SFML_API bool ImageButton(const sf::Sprite& sprite, const int framePadding = -1,
        const sf::Color& bgColor = sf::Color::Transparent,
        const sf::Color& tintColor = sf::Color::White);
    IMGUI_SFML_API bool ImageButton(const sf::Sprite& sprite, const sf::Vector2f& size, const int framePadding = -1,
        const sf::Color& bgColor = sf::Color::Transparent,
        const sf::Color& tintColor = sf::Color::White);

    // Draw_list overloads. All positions are in relative coordinates (relative to top-left of the current window)
    IMGUI_SFML_API void DrawLine(const sf::Vector2f& a, const sf::Vector2f& b, const sf::Color& col, float thickness = 1.0f);
    IMGUI_SFML_API void DrawRect(const sf::FloatRect& rect, const sf::Color& color, float rounding = 0.0f, int rounding_corners = 0x0F, float thickness = 1.0f);
    IMGUI_SFML_API void DrawRectFilled(const sf::FloatRect& rect, const sf::Color& color, float rounding = 0.0f, int rounding_corners = 0x0F);
}

#endif //# IMGUI_SFML_H
