#ifndef RENDERER_H
#define RENDERER_H

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <array>
#include <vector>
#include <unordered_map>
#include <optional>

/**
 * RENDERER UI
 * Questa classe si occupa della gestione grafica e dell'input hardware
 * del sistema chip8, tramite api sfml.
 * L'oggetto incapsula sfml, per richiamare l'esecuzione su una finestra di eventi sfml
 * all'interno del main loop.
 */

class Pixel : public sf::Drawable, public sf::Transformable
{
public: 
    explicit Pixel(const float size = 0) : m_shape({size, size}) 
    {
        m_shape.setFillColor(sf::Color::Black);
    }
    ~Pixel() = default;

    sf::Vector2f getSize() const {return m_shape.getSize();}
    
    void setPosition(const sf::Vector2f pos) {sf::Transformable::setPosition(pos);}

    void toggle(const bool b) {m_isActive = b;}

private:
    mutable sf::RectangleShape m_shape;

    bool m_isActive = false;

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override
    {
        if(m_isActive) m_shape.setFillColor(sf::Color::White);
        else m_shape.setFillColor(sf::Color::Black);

        m_shape.setPosition(getPosition());
        target.draw(m_shape, states);
    }
};

class SfDisplay : public sf::Drawable, public sf::Transformable
{
public:
    SfDisplay() = default;
    SfDisplay(const int pixel_size)
    {
        for(auto& arr : m_pixels) {
            for(auto& p : arr) {
                p = Pixel(pixel_size);
            }
        }

        generatePixelGrid();
    }
    ~SfDisplay() = default;

    void updateState(const std::array<std::array<bool, 64>, 32>& state)
    {
        for(std::size_t i = 0; i < state.size(); ++i) 
        {
            for(std::size_t j = 0; j < state[i].size(); ++j) 
            {
                m_pixels[i][j].toggle(state[i][j]);
            }
        }
    }

private:
    std::array<std::array<Pixel, 64>, 32> m_pixels;

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override 
    {
        for(const auto& line : m_pixels) {
            for(const auto& p : line) {
                target.draw(p, states);
            }
        }
    }
    
    void generatePixelGrid()
    {
        const float start_x = getPosition().x;
        const float start_y = getPosition().y;

        for(std::size_t i = 0; i < m_pixels.size(); ++i)
        {
            for(std::size_t j = 0; j < m_pixels[i].size(); ++j)
            {
                m_pixels[i][j].setPosition({
                    start_x + j * m_pixels[i][j].getSize().x,
                    start_y + i * m_pixels[i][j].getSize().y
                });
            }
        }
    }
};

/* codici dei tasti disponibili nel chip8 */
enum class KeyCode 
{
    _0 = 0x0, _1 = 0x1, _2 = 0x2, _3 = 0x3,
    _4 = 0x4, _5 = 0x5, _6 = 0x6, _7 = 0x7,
    _8 = 0x8, _9 = 0x9, _A = 0xA, _B = 0xB,
    _C = 0xC, _D = 0xD, _E = 0xE, _F = 0xF
};

/*
Tastierino CHIP-8:        Tastiera PC:
1 2 3 C                   1 2 3 4
4 5 6 D                   Q W E R
7 8 9 E                   A S D F
A 0 B F                   Z X C V
*/

static const std::unordered_map<sf::Keyboard::Key, KeyCode> keycodes = 
{
    {sf::Keyboard::Num1, KeyCode::_1},
    {sf::Keyboard::Num2, KeyCode::_2},
    {sf::Keyboard::Num3, KeyCode::_3},
    {sf::Keyboard::Num4, KeyCode::_C},
    {sf::Keyboard::Q,    KeyCode::_4},
    {sf::Keyboard::W,    KeyCode::_5},
    {sf::Keyboard::E,    KeyCode::_6},
    {sf::Keyboard::R,    KeyCode::_D},
    {sf::Keyboard::A,    KeyCode::_7},
    {sf::Keyboard::S,    KeyCode::_8},
    {sf::Keyboard::D,    KeyCode::_9},
    {sf::Keyboard::F,    KeyCode::_E},
    {sf::Keyboard::Z,    KeyCode::_A},
    {sf::Keyboard::X,    KeyCode::_0},
    {sf::Keyboard::C,    KeyCode::_B},
    {sf::Keyboard::V,    KeyCode::_F},
};

struct InputKey
{
    bool pressed;
    KeyCode key;
};

static const int display_scale = 10;

class Renderer 
{
public:
    Renderer()
        : m_window(sf::VideoMode(64 * display_scale, 32 * display_scale), "window - chip-8", sf::Style::Close)
        , m_display(display_scale)
    {
        m_window.setVisible(false);
        m_running = true;
    }
    ~Renderer() = default;

    void showWindow() {m_window.setVisible(true);}

    bool isOpen() const {return m_running;}

    // main loop della finestra sfml
    void executeWindowFrame()
    {
        sf::Event event;
        while(m_window.pollEvent(event)) {

            if (event.type == sf::Event::Closed) 
            {
                m_window.close(); //close with "X" button
                m_running = false;
            }

            if (event.type == sf::Event::Resized) {
                sf::FloatRect visibleArea(0, 0, event.size.width, event.size.height);
                m_window.setView(sf::View(visibleArea));
            }
        }

        for(const auto& [sfml_key, chip8_key] : keycodes)
        {
            bool isPressed = sf::Keyboard::isKeyPressed(sfml_key);
            m_keys[static_cast<int>(chip8_key)].key = chip8_key;
            m_keys[static_cast<int>(chip8_key)].pressed = isPressed;
        }

        m_window.clear(sf::Color(100, 100, 100));
        m_window.draw(m_display);
        m_window.display();
    }

    //update del display
    void updateDisplayState(const std::array<std::array<bool, 64>, 32>& display) 
    {
        m_display.updateState(display);
    }

    std::array<InputKey, 16> getKeys() const {
        return m_keys;
    }

    bool isPressed(int code) const {
        return m_keys[code].pressed;
    }

    std::optional<int> getPressedKey() const {
        for(int i = 0; i < 16; ++i) {
            if(m_keys[i].pressed) return i;
        }
        return std::nullopt;
    }

private:
    sf::RenderWindow m_window;
    SfDisplay m_display;
    std::array<InputKey, 16> m_keys;
    bool m_running = false;
};

#endif //RENDERER_H