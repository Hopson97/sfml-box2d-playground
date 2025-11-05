#include <iostream>
#include <print>
#include <random>

#include <SFML/Graphics/ConvexShape.hpp>
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
    /// Convert between Box2D and SFML sizes, so 1 meter = 'SCALE' pixels
    constexpr float SCALE = 10.f;

    /// Size of the dynamic_boxes
    constexpr float DYNAMIC_BOX_SIZE = 1.0f;

    /// The number of dynamic_boxes to spawn at the start
    constexpr int BOX_COUNT = 1;

    /// Converts a Box2D vector to a SFML vector scaled from meters to pixels
    sf::Vector2f to_sfml_position(b2Vec2 box2d_position, int window_height);

    /// Converts a Box2D size to SFML size for rendering
    sf::Vector2f to_sfml_size(b2Vec2 box2d_size);

    /// Creates a random vec2
    b2Vec2 create_random_b2vec(float x_min = 0.0f, float x_max = 3.0f, float y_min = 20.0f,
                               float y_max = 3.0f);

    /// Generate a random colour
    sf::Color random_colour();

    struct Box
    {
        b2Vec2 size;
        b2BodyId body;
        sf::Color colour;
    };

    struct PhysicsObject
    {
        b2BodyId body;
        sf::ConvexShape shape;
    };

    /// Creates a box that has physics applied to it at a random position
    Box create_box(b2WorldId world);

    /// Create a box that does not move
    Box create_static_box(b2WorldId world, b2Vec2 size, sf::Vector2f position);

    PhysicsObject create_special(b2WorldId world, const std::vector<b2Vec2>& points);

    void apply_explosion(b2BodyId body, float explode_strength, sf::Vector2f explosion_position);

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

    // Create static boxes
    std::vector<Box> static_boxes = {
        create_static_box(world, {60, 1}, {61, 2}), create_static_box(world, {1, 30}, {2, 33}),
        create_static_box(world, {2, 2}, {50, 50}), create_static_box(world, {2, 2}, {40, 10}),
        create_static_box(world, {2, 2}, {10, 10}),
    };

    // Create dynamic boxes
    std::vector<Box> dynamic_boxes;
    for (int i = 0; i < BOX_COUNT; i++)
    {
        dynamic_boxes.push_back(create_box(world));
    }

    auto special = create_special(world, {{-5.0f, 0.0f}, {5.0f, 0.0f}, {0.0f, 5.0f}});

    sf::RectangleShape box_rectangle;
    box_rectangle.setOutlineColor(sf::Color::White);
    box_rectangle.setOutlineThickness(1.0f);

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
                // Push the dynamic_boxes away from where the mouse is clicked
                if (auto mouse_click = event->getIf<sf::Event::MouseButtonReleased>())
                {
                    // Scale down the mouse click to "meters"
                    auto mouse_position = sf::Vector2f{
                        mouse_click->position.x / SCALE,
                        (window.getSize().y - mouse_click->position.y) / SCALE,
                    };

                    // Move all the dynamic_boxes away from the mouse point by applying a linear
                    // impulse
                    for (auto& box : dynamic_boxes)
                    {
                        apply_explosion(box.body, explode_strength, mouse_position);
                    }

                    apply_explosion(special.body, explode_strength, mouse_position);
                }
            }
        }
        ImGui::SFML::Update(window, clock.restart());
        window.clear(sf::Color::Black);

        // Update the world and do the physics simulation
        {
            auto& section = profiler.begin_section("Update");
            b2World_Step(world, timestep, sub_steps);
            section.end_section();
        }

        {
            auto& section = profiler.begin_section("Render");
            // Render the static geometry
            for (auto& box : static_boxes)
            {
                auto position = b2Body_GetPosition(box.body);

                box_rectangle.setRotation(sf::Angle::Zero);
                box_rectangle.setPosition(to_sfml_position(position, window.getSize().y));
                box_rectangle.setSize(to_sfml_size(box.size));
                box_rectangle.setOrigin(box_rectangle.getSize() / 2.f);
                box_rectangle.setFillColor(box.colour);
                window.draw(box_rectangle);
            }

            // Draw all the dynamic_boxes
            for (auto& box : dynamic_boxes)
            {
                // Get the position and rotation from box2d
                auto radians = b2Rot_GetAngle(b2Body_GetRotation(box.body));
                auto position = b2Body_GetPosition(box.body);

                box_rectangle.setRotation(sf::radians(radians));
                box_rectangle.setPosition(to_sfml_position(position, window.getSize().y));
                box_rectangle.setSize(to_sfml_size(box.size));
                box_rectangle.setOrigin(box_rectangle.getSize() / 2.f);
                box_rectangle.setFillColor(box.colour);
                window.draw(box_rectangle);
            }

            // Draw speical shapes
            {
                auto radians = b2Rot_GetAngle(b2Body_GetRotation(special.body));
                auto position = b2Body_GetPosition(special.body);

                special.shape.setRotation(sf::radians(radians));
                special.shape.setPosition(to_sfml_position(position, window.getSize().y));
                window.draw(special.shape);

                box_rectangle.setRotation(sf::Angle::Zero);
                box_rectangle.setPosition(special.shape.getPosition());
                box_rectangle.setSize({2,2});
                box_rectangle.setOrigin({0,0});
                box_rectangle.setFillColor(sf::Color::Red);
                window.draw(box_rectangle);
            }

            section.end_section();
        }

        // Show profiler
        profiler.end_frame();
        if (show_debug_info)
        {
            profiler.gui();
        }

        // Gui for controlling simulation aspects and restting the scene
        if (ImGui::Begin("Config"))
        {
            ImGui::SliderFloat("Explode Stength", &explode_strength, 1.0f, 10000.0f);

            if (ImGui::Button("Reset"))
            {
                for (auto& box : dynamic_boxes)
                {
                    b2Body_SetLinearVelocity(box.body, {0, 0});
                    b2Body_SetAngularVelocity(box.body, 0);
                    b2Body_SetTransform(box.body, create_random_b2vec(), b2Rot_identity);
                }
                special = create_special(world, {{-5.0f, 0.0f}, {5.0f, 0.0f}, {0.0f, 5.0f}});
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
    for (auto& box : dynamic_boxes)
    {
        b2DestroyBody(box.body);
    }
    for (auto& box : static_boxes)
    {
        b2DestroyBody(box.body);
    }
    b2DestroyWorld(world);
}

namespace
{
    sf::Vector2f to_sfml_position(b2Vec2 box2d_position, int window_height)
    {
        // Box2D defines the bottom left as the origin (so Y is up), so the Y must be inverted
        return {
            box2d_position.x * SCALE,
            window_height - box2d_position.y * SCALE,
        };
    }

    sf::Vector2f to_sfml_size(b2Vec2 box_size)
    {
        // Box2d objects are defined using HALF the size given, so when converting to SFML the
        // result must be * 2
        return {box_size.x * SCALE * 2, box_size.y * SCALE * 2};
    }

    b2Vec2 create_random_b2vec(float x_min, float x_max, float y_min, float y_max)
    {
        static std::random_device rd;
        static std::mt19937 rng(rd());
        return {
            std::uniform_real_distribution(10.0f, 50.0f)(rng),
            std::uniform_real_distribution(10.0f, 50.0f)(rng),
        };
    }

    sf::Color random_colour()
    {
        // constexpr static std::array<sf::Color, 7> COLOURS{
        //     sf::Color::White,  sf::Color::Red,     sf::Color::Green, sf::Color::Blue,
        //     sf::Color::Yellow, sf::Color::Magenta, sf::Color::Cyan,
        // };
        static std::random_device rd;
        static std::mt19937 rng(rd());
        return {
            static_cast<uint8_t>(std::uniform_int_distribution<int>(0, 255)(rng)),
            static_cast<uint8_t>(std::uniform_int_distribution<int>(0, 255)(rng)),
            static_cast<uint8_t>(std::uniform_int_distribution<int>(0, 255)(rng)),
        };
    }

    Box create_box(b2WorldId world)
    {
        b2BodyDef body = b2DefaultBodyDef();
        body.type = b2_dynamicBody;
        body.position = create_random_b2vec();

        // As this example has no gravity, damping needs to be applied or objects will float and
        // spin forever
        body.linearDamping = 1.0f;
        body.angularDamping = 1.0f;

        b2ShapeDef shape = b2DefaultShapeDef();
        shape.density = 1.0f;
        shape.material.friction = 0.3f;

        b2BodyId body_id = b2CreateBody(world, &body);
        b2Polygon box = b2MakeBox(DYNAMIC_BOX_SIZE, DYNAMIC_BOX_SIZE);
        b2CreatePolygonShape(body_id, &shape, &box);

        return {
            .size = {DYNAMIC_BOX_SIZE, DYNAMIC_BOX_SIZE},
            .body = body_id,
            .colour = random_colour(),
        };
    }

    Box create_static_box(b2WorldId world, b2Vec2 size, sf::Vector2f position)
    {
        b2BodyDef body = b2DefaultBodyDef();
        body.type = b2_staticBody;
        body.position = {position.x, position.y};

        b2Polygon box = b2MakeBox(size.x, size.y);
        b2ShapeDef shape = b2DefaultShapeDef();
        b2BodyId body_id = b2CreateBody(world, &body);

        b2CreatePolygonShape(body_id, &shape, &box);

        return {
            .size = size,
            .body = body_id,
            .colour = sf::Color::Green,
        };
    }

    b2Vec2 computeCentroid(const std::vector<b2Vec2>& verts)
    {
        float area = 0.f;
        float cx = 0.f, cy = 0.f;

        for (int i = 0; i < verts.size(); i++)
        {
            b2Vec2 p0 = verts[i];
            b2Vec2 p1 = verts[(i + 1) % verts.size()];

            float cross = p0.x * p1.y - p1.x * p0.y; // cross product
            area += cross;
            cx += (p0.x + p1.x) * cross;
            cy += (p0.y + p1.y) * cross;
        }
        area *= 0.5f;
        cx /= (6.f * area);
        cy /= (6.f * area);

        return {cx, cy};
    }

    PhysicsObject create_special(b2WorldId world, const std::vector<b2Vec2>& points)
    {
        b2BodyDef body = b2DefaultBodyDef();
        body.type = b2_dynamicBody;
        body.position = create_random_b2vec();
        body.linearDamping = 1.0f;
        body.angularDamping = 1.0f;

        b2ShapeDef shape = b2DefaultShapeDef();
        shape.density = 1.0f;
        shape.material.friction = 0.3f;

        b2BodyId body_id = b2CreateBody(world, &body);
        b2Hull hull = b2ComputeHull(points.data(), points.size());
        b2Polygon polygon = b2MakePolygon(&hull, 0);
        b2CreatePolygonShape(body_id, &shape, &polygon);

        PhysicsObject object;
        object.shape.setPointCount(points.size());
        for (std::size_t i = 0; i < points.size(); i++)
        {
            object.shape.setPoint(i, {points[i].x * SCALE, points[i].y * SCALE});
        }
        object.shape.setFillColor(random_colour());
        object.shape.setOutlineColor(sf::Color::White);
        object.shape.setOutlineThickness(1.0f);
        object.body = body_id;

        b2Vec2 com = b2Body_GetLocalCenterOfMass(body_id);
        object.shape.setOrigin({com.x * SCALE, com.y * SCALE});

        return object;
    }

    void apply_explosion(b2BodyId body, float explode_strength, sf::Vector2f explosion_position)
    {
        auto body_pos = b2Body_GetPosition(body);
        auto body_position = sf::Vector2f{body_pos.x, body_pos.y};

        auto diff = body_position - explosion_position;
        auto distance = diff.lengthSquared() / 2.0f;
        if (distance > 0.001f)
        {
            // The strength depends with distance
            diff *= explode_strength / distance;
            b2Body_ApplyLinearImpulse(body, {diff.x, diff.y}, body_pos, true);
        }
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