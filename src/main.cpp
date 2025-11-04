#include <iostream>
#include <print>
#include <random>

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include <box2d/box2d.h>
#include <imgui.h>
#include <imgui_sfml/imgui-SFML.h>

#include "Util/Keyboard.h"
#include "Util/Profiler.h"

namespace
{
    /// Convert between Box2D and SFML sizes, so 1 meter = 20 pixels
    constexpr float SCALE = 20.f;

    /// Size of the boxes
    constexpr float DYNAMIC_BOX_SIZE = 1.0f;
    constexpr float GROUND_WIDTH = 50.0f;
    constexpr float GROUND_HEIGHT = 8.0f;

    /// The number of boxes to spawn at the start
    constexpr int BOX_COUNT = 50;

    /// Converts a Box2D vector to a SFML vector scaled from meters to pixels
    sf::Vector2f to_sfml_position(const sf::RenderWindow& window, b2Vec2 box2d_position);

    /// Creates a random vec2
    b2Vec2 create_random_b2vec(float x_min = 0.0f, float x_max = 3.0f, float y_min = 20.0f,
                               float y_max = 3.0f);

    /// Creates a box that has physics applied to it at a random position
    b2BodyId create_box(b2WorldId world);

    /// Create a static ground plane
    b2BodyId create_ground(b2WorldId world);

    /// Window event handing
    void handle_event(const sf::Event& event, sf::Window& window, bool& show_debug_info,
                      bool& close_requested);
} // namespace

int main()
{
    sf::RenderWindow window(sf::VideoMode({1600, 900}), "Box2D 3 + SFML 3", sf::State::Windowed,
                            {.antiAliasingLevel = 4});

    window.setVerticalSyncEnabled(true);
    if (!ImGui::SFML::Init(window))
    {
        std::println(std::cerr, "Failed to init ImGUI::SFML.");
        return EXIT_FAILURE;
    }

    Profiler profiler;
    bool show_debug_info = false;

    Keyboard keyboard;

    /// Define the world
    b2WorldDef world_def = b2DefaultWorldDef();
    world_def.gravity = {0.0f, -10.0f};
    world_def.gravity = {0.0f, 0.0f};
    b2WorldId world = b2CreateWorld(&world_def);

    // Create the ground
    auto ground_id = create_ground(world);

    // Create boxes
    std::vector<b2BodyId> bodies;
    for (int i = 0; i < BOX_COUNT; i++)
    {
        bodies.push_back(create_box(world));
    }

    // Note when defining object size, the sizes MUST be * 2.0
    // This is because Box2d defines objects starting at their center, so a box of size = 1 is
    // actually 2m across
    sf::RectangleShape ground_rect({GROUND_WIDTH * SCALE * 2, GROUND_HEIGHT * SCALE * 2});
    ground_rect.setOrigin(ground_rect.getSize() / 2.f);
    ground_rect.setFillColor(sf::Color::Green);

    sf::RectangleShape box_rect({DYNAMIC_BOX_SIZE * SCALE * 2, DYNAMIC_BOX_SIZE * SCALE * 2});
    box_rect.setOrigin(box_rect.getSize() / 2.f);
    box_rect.setFillColor(sf::Color::Red);

    sf::Clock clock;

    // Parameters used for the box2d simultion
    auto timestep = 1.f / 60.f;
    auto sub_steps = 4;
    auto explode_strength = 50.0f;
    while (window.isOpen())
    {
        bool close_requested = false;
        while (auto event = window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(window, *event);
            keyboard.update(*event);
            handle_event(*event, window, show_debug_info, close_requested);

            if (!ImGui::GetIO().WantCaptureMouse)
            {
                // Push the boxes away from where the mouse is clicked
                if (auto mouse_click = event->getIf<sf::Event::MouseButtonReleased>())
                {
                    // Scale down the mouse click to "meters"
                    auto mouse_position = sf::Vector2f{
                        mouse_click->position.x / SCALE,
                        (window.getSize().y - mouse_click->position.y) / SCALE,
                    };

                    // Move all the boxes away from the mouse point by applying a linear impulse
                    for (auto& body : bodies)
                    {
                        auto body_pos = b2Body_GetPosition(body);
                        auto body_position = sf::Vector2f{body_pos.x, body_pos.y};

                        auto diff = body_position - mouse_position;
                        auto distance = diff.length();
                        if (distance > 0.001f)
                        {
                            // The strength depends with distance
                            diff *= explode_strength / distance;
                            b2Body_ApplyLinearImpulse(body, {diff.x, diff.y}, body_pos, true);
                        }
                    }
                }
            }
        }
        window.clear(sf::Color::Black);

        // Update the world and do the physics simulation
        ImGui::SFML::Update(window, clock.restart());
        b2World_Step(world, timestep, sub_steps);

        // Render the ground
        b2Vec2 groundPos = b2Body_GetPosition(ground_id);
        ground_rect.setPosition(to_sfml_position(window, groundPos));
        window.draw(ground_rect);

        // Draw all the boxes
        for (auto& body : bodies)
        {
            // Get the position and rotation from box2d
            auto radians = b2Rot_GetAngle(b2Body_GetRotation(body));
            auto position = b2Body_GetPosition(body);

            // Convert and set to SFML units
            box_rect.setRotation(sf::radians(radians));
            box_rect.setPosition(to_sfml_position(window, position));
            window.draw(box_rect);
        }

        // Show profiler
        profiler.end_frame();
        if (show_debug_info)
        {
            profiler.gui();
        }

        if (ImGui::Begin("Config"))
        {
            ImGui::SliderFloat("Explode Stength", &explode_strength, 1.0f, 1000.0f);

            if (ImGui::Button("Reset"))
            {
                for (auto& body : bodies)
                {
                    b2Body_SetLinearVelocity(body, {0, 0});
                    b2Body_SetAngularVelocity(body, 0);
                    b2Body_SetTransform(body, create_random_b2vec(), b2Rot_identity);
                }
            }
        }
        ImGui::End();

        // End frame
        ImGui::SFML::Render(window);
        window.display();
        if (close_requested)
        {
            window.close();
        }
    }

    // Cleanup
    ImGui::SFML::Shutdown(window);
    for (auto& body : bodies)
    {
        b2DestroyBody(body);
    }
    b2DestroyWorld(world);
}

namespace
{
    sf::Vector2f to_sfml_position(const sf::RenderWindow& window, b2Vec2 box2d_position)
    {
        // Box2D has the coords such that 0,0 is the bottom left, so the Y must be inverted
        return {
            box2d_position.x * SCALE,
            window.getSize().y - box2d_position.y * SCALE,
        };
    }

    b2Vec2 create_random_b2vec(float x_min, float x_max, float y_min, float y_max)
    {
        static std::random_device rd;
        static std::mt19937 rng(rd());
        return {
            std::uniform_real_distribution(0.0f, 30.0f)(rng),
            std::uniform_real_distribution(10.0f, 30.0f)(rng),
        };
    }

    b2BodyId create_box(b2WorldId world)
    {

        // Define a box to fall
        b2BodyDef body = b2DefaultBodyDef();
        body.type = b2_dynamicBody;
        body.position = create_random_b2vec();

        // As this example has no gravity, damping needs to be applied or objects will float and
        // spin forever
        body.linearDamping = 1.0f;
        body.angularDamping = 1.0f;

        b2ShapeDef shape = b2DefaultShapeDef();
        shape.density = 5.0f;
        shape.material.friction = 0.3f;

        b2BodyId body_id = b2CreateBody(world, &body);
        b2Polygon box = b2MakeBox(DYNAMIC_BOX_SIZE, DYNAMIC_BOX_SIZE);
        b2CreatePolygonShape(body_id, &shape, &box);

        return body_id;
    }

    b2BodyId create_ground(b2WorldId world)
    {
        b2BodyDef body = b2DefaultBodyDef();
        body.position = {0.0f, 1.0f};

        b2Polygon box = b2MakeBox(GROUND_WIDTH, GROUND_HEIGHT);
        b2ShapeDef shape = b2DefaultShapeDef();
        b2BodyId ground_id = b2CreateBody(world, &body);
        b2CreatePolygonShape(ground_id, &shape, &box);

        return ground_id;
    }

    void handle_event(const sf::Event& event, sf::Window& window, bool& show_debug_info,
                      bool& close_requested)
    {
        if (event.is<sf::Event::Closed>())
        {
            close_requested = true;
        }
        else if (auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            switch (key->code)
            {
                case sf::Keyboard::Key::Escape:
                    close_requested = true;
                    break;

                case sf::Keyboard::Key::F1:
                    show_debug_info = !show_debug_info;
                    break;

                default:
                    break;
            }
        }
    }
} // namespace