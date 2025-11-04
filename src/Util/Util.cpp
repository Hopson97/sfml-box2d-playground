#include "Util.h"

#include <array>
#include <fstream>
#include <iostream>

#include <SFML/Graphics/Image.hpp>

namespace
{
    struct ErrorTexture
    {
        constexpr static std::size_t CHANNEL_R = 0;
        constexpr static std::size_t CHANNEL_G = 1;
        constexpr static std::size_t CHANNEL_B = 2;
        constexpr static std::size_t CHANNEL_A = 3;
        sf::Texture texture;

        ErrorTexture()
        {
            std::array<std::uint8_t, 16 * 16 * 4> pixels;

            sf::Image image({16, 16}, pixels.data());
            for (unsigned y = 0; y < 16; y++)
            {
                for (unsigned x = 0; x < 16; x++)
                {
                    std::uint8_t r = y % 2 == 0 ? 255 : 0;
                    std::uint8_t g = 0;
                    std::uint8_t b = x % 2 == 0 ? 255 : 0;
                    image.setPixel({x, y}, {r, g, b});
                }
            }

            if (!texture.loadFromImage(image))
            {
                std::println(std::cerr, "Something has gone very wrong\n");
            }
        }
    };
} // namespace

void load_texture(sf::Texture& texture, const std::filesystem::path& file_path)
{
    static ErrorTexture error_texture;

    if (!texture.loadFromFile(file_path))
    {
        texture = error_texture.texture;
    }
}

std::string read_file_to_string(const std::filesystem::path& file_path)
{
    std::ifstream in_file(file_path);

    if (!in_file)
    {
        std::cerr << "Failed to open file " << file_path << std::endl;
        return "";
    }

    std::string content((std::istreambuf_iterator<char>(in_file)),
                        (std::istreambuf_iterator<char>()));

    return content;
}

std::vector<std::string> split_string(const std::string& string, char delim)
{
    std::vector<std::string> tokens;

    std::stringstream stream(string);
    std::string token;
    while (std::getline(stream, token, delim))
    {
        tokens.push_back(token);
    }
    return tokens;
}