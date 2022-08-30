// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 Intel Corporation. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include "example-imgui.hpp"

// 3rd party header for writing png files
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <thread>
#include <chrono>

using namespace std;
using namespace std::chrono;

/*
 This example introduces the concept of spatial stream alignment.
 For example usecase of alignment, please check out align-advanced and measure demos.
 The need for spatial alignment (from here "align") arises from the fact
 that not all camera streams are captured from a single viewport.
 Align process lets the user translate images from one viewport to another.
 That said, results of align are synthetic streams, and suffer from several artifacts:
 1. Sampling - mapping stream to a different viewport will modify the resolution of the frame
               to match the resolution of target viewport. This will either cause downsampling or
               upsampling via interpolation. The interpolation used needs to be of type
               Nearest Neighbor to avoid introducing non-existing values.
 2. Occlussion - Some pixels in the resulting image correspond to 3D coordinates that the original
               sensor did not see, because these 3D points were occluded in the original viewport.
               Such pixels may hold invalid texture values.
*/

// This example assumes camera with depth and color
// streams, and direction lets you define the target stream
enum class direction
{
    to_depth,
    to_color
};

// Forward definition of UI rendering, implemented below
void render_slider(rect location, float* alpha, direction* dir, bool* cap, int num_images, bool* ft, bool* align, bool* colorizing, bool* rendering);

inline std::string get_profile_description(const rs2::stream_profile& profile)
{
    std::stringstream ss;
    ss << profile.stream_name() << " " << profile.format() << " ";

    if (auto vp = profile.as<rs2::video_stream_profile>())
        ss << vp.width() << "x" << vp.height();

    ss << "@" << profile.fps() << " ";
    ss << std::endl;
    return ss.str().c_str();
}

int main(int argc, char * argv[]) try
{
    rs2::context ctx;
    auto devs = ctx.query_devices();

    if (devs.size() < 1)
        return EXIT_SUCCESS;

    std::string serial;
    if (!device_with_streams({ RS2_STREAM_COLOR,RS2_STREAM_DEPTH }, serial))
        return EXIT_SUCCESS;

    std::string device_name = "";
    std::string device_pid = "";

    for (rs2::device dev : devs)
    {
        std::string device_sn = dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);

        if (device_sn == serial)
        {
            device_name = dev.get_info(RS2_CAMERA_INFO_NAME);
            device_pid = dev.get_info(RS2_CAMERA_INFO_PRODUCT_ID);
        }
    }

    // options
    bool enable_filter = false;      // Filter on or off
    bool enable_align = true;        // Enable depth to color or color to depth align
    bool enable_colorizer = true;    // Enable depth colorizer
    bool enable_rendering = true;    // Rendering depth and RGB images


    // Declare filters
    rs2::temporal_filter filter;
    filter.set_option(RS2_OPTION_FRAMES_QUEUE_SIZE, 16);
    filter.set_option(RS2_OPTION_FILTER_SMOOTH_ALPHA, 0.08);
    filter.set_option(RS2_OPTION_FILTER_SMOOTH_DELTA, 80);
    filter.set_option(RS2_OPTION_HOLES_FILL, 8);

    rs2::colorizer c;                     // Helper to colorize depth images
    texture depth_image, color_image;     // Helpers for renderig images
    int captured_image_counter = 0;       // number of images user saved by clicking the capture button

    // Create a pipeline to easily configure and start the camera
    rs2::pipeline pipe;
    rs2::config cfg;
    if (!serial.empty())
        cfg.enable_device(serial);

    if (device_pid == "0B4F")  // D436
    {
        cfg.enable_stream(RS2_STREAM_DEPTH, 848, 480, RS2_FORMAT_Z16, 15);
        //	cfg.enable_stream(RS2_STREAM_COLOR, 1920, 1080, RS2_FORMAT_RGB8, 15);
        //	cfg.enable_stream(RS2_STREAM_COLOR, 2048, 1536, RS2_FORMAT_RGB8, 15);
        cfg.enable_stream(RS2_STREAM_COLOR, 4160, 3120, RS2_FORMAT_RGB8, 15);
    }
    else if (device_pid == "0B07" || device_pid == "0B3A")  // D435 and D435i
    {
        cfg.enable_stream(RS2_STREAM_DEPTH, 1280, 720, RS2_FORMAT_Z16, 15);
        cfg.enable_stream(RS2_STREAM_COLOR, 1920, 1080, RS2_FORMAT_RGB8, 15);
    }
    else
    {
        cfg.enable_stream(RS2_STREAM_DEPTH, 640, 480, RS2_FORMAT_Z16, 15);
        cfg.enable_stream(RS2_STREAM_COLOR, 640, 480, RS2_FORMAT_RGB8, 15);
    }

    rs2::pipeline_profile pf = pipe.start(cfg);
    std::string sp_description;
    std::vector<rs2::stream_profile> profiles = pf.get_streams();

    for (rs2::stream_profile sp : profiles)
    {
        sp_description += get_profile_description(sp);
    }

    // Create and initialize GUI related objects
    std::string title = "RealSense Align Example - " + device_name + " " + sp_description;

    window app(1920, 1080, title.c_str()); // Simple window handling
    ImGui_ImplGlfw_Init(app, false);      // ImGui library intializition

    // Define two align objects. One will be used to align
    // to depth viewport and the other to color.
    // Creating align object is an expensive operation
    // that should not be performed in the main loop
    rs2::align align_to_depth(RS2_STREAM_DEPTH);
    rs2::align align_to_color(RS2_STREAM_COLOR);

    float       alpha = 0.5f;               // Transparancy coefficient
    direction   dir = direction::to_color;  // Alignment direction

    while (app) // Application still alive?
    {
#if 1
		static int count = 0;
		static int total_time = 0;
		time_point<high_resolution_clock> beforeTime = high_resolution_clock::now();
#endif

        // Using the align object, we block the application until a frameset is available
        rs2::frameset frameset = pipe.wait_for_frames();

        if (enable_align)
        {
            if (dir == direction::to_depth)
            {
                // Align all frames to depth viewport
                frameset = align_to_depth.process(frameset);
            }
            else
            {
                // Align all frames to color viewport
                frameset = align_to_color.process(frameset);
            }
        }

        // With the aligned frameset we proceed as usual
        auto depth = frameset.get_depth_frame();
        auto color = frameset.get_color_frame();

        rs2::frame processed_depth = depth;

        if (enable_filter)
        {
            processed_depth = filter.process(depth);
        }

        auto colorized_depth = processed_depth;

        if (enable_colorizer)
            colorized_depth = c.colorize(processed_depth);

#if 1
        count++;
        time_point<high_resolution_clock> currentTime = high_resolution_clock::now();
        milliseconds passedTime = duration_cast<milliseconds>(currentTime - beforeTime);
        total_time += passedTime.count();

        if (count == 15)
        {
            int avg_time = total_time / count;
            std::cout << "average frame time: " << avg_time << " ms\n";

            total_time = 0;
            count = 0;
        }
#endif

        // rendering
        glEnable(GL_BLEND);
        // Use the Alpha channel for blending
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (enable_rendering)
        {
            if (dir == direction::to_depth)
            {
                // When aligning to depth, first render depth image
                // and then overlay color on top with transparancy
                if (enable_colorizer)
                    depth_image.render(colorized_depth, { 0, 0, app.width(), app.height() });

                color_image.render(color, { 0, 0, app.width(), app.height() }, alpha);
            }
            else
            {
                // When aligning to color, first render color image
                // and then overlay depth image on top
                color_image.render(color, { 0, 0, app.width(), app.height() });
                if (enable_colorizer)
                    depth_image.render(colorized_depth, { 0, 0, app.width(), app.height() }, 1 - alpha);
            }
        }
        glColor4f(1.f, 1.f, 1.f, 1.f);
        glDisable(GL_BLEND);

        bool capture = false;

        // Render the UI:
        ImGui_ImplGlfw_NewFrame(1);
        render_slider({ 15.f, app.height() - 60, app.width() - 30, app.height() }, &alpha, &dir, &capture, captured_image_counter, &enable_filter, &enable_align, &enable_colorizer, &enable_rendering);
        ImGui::Render();

        if (capture == true)
        {
            // Write images to disk
            std::stringstream depth_png_file;
            depth_png_file << "rs-align-" << captured_image_counter << "-depth.png";
            stbi_write_png(depth_png_file.str().c_str(), depth.get_width(), depth.get_height(), depth.get_bytes_per_pixel(), depth.get_data(), depth.get_stride_in_bytes());

            std::stringstream color_png_file;
            color_png_file << "rs-align-" << captured_image_counter << "-color.png";
            stbi_write_png(color_png_file.str().c_str(), color.get_width(), color.get_height(), color.get_bytes_per_pixel(), color.get_data(), color.get_stride_in_bytes());

            captured_image_counter++;
        }
    }

    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}

void render_slider(rect location, float* alpha, direction* dir, bool* cap, int num_images, bool* ft, bool* align, bool* colorizing, bool* rendering)
{
    static const int flags = ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove;

    ImGui::SetNextWindowPos({ location.x, location.y });
    ImGui::SetNextWindowSize({ location.w, location.h });

    // Render transparency slider:
    ImGui::Begin("slider", nullptr, flags);
    ImGui::PushItemWidth(-1);
    ImGui::SliderFloat("##Slider", alpha, 0.f, 1.f);
    ImGui::PopItemWidth();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Texture Transparancy: %.3f", *alpha);

    // Render checkboxes:
	bool filter_on = *ft;
	if (ImGui::Checkbox("Enable Filters", &filter_on))
	{
		*ft = filter_on;
	}

	ImGui::SameLine();
	bool colorizer_on = *colorizing;

	if (ImGui::Checkbox("Enable Colorizer", &colorizer_on))
	{
		*colorizing = colorizer_on;
	}

	ImGui::SameLine();
	bool align_on = *align;

	if (ImGui::Checkbox("Enable Align", &align_on))
	{
		*align = align_on;
	}

	if (align_on)
	{
		bool to_depth = (*dir == direction::to_depth);
		bool to_color = (*dir == direction::to_color);

		ImGui::SameLine();
		if (ImGui::Checkbox("Align To Depth", &to_depth))
		{
			*dir = to_depth ? direction::to_depth : direction::to_color;
		}

		ImGui::SameLine();
		if (ImGui::Checkbox("Align To Color", &to_color))
		{
			*dir = to_color ? direction::to_color : direction::to_depth;
		}
	}

    ImGui::SameLine();
    bool rendering_on = *rendering;

    if (ImGui::Checkbox("Enable Rendering", &rendering_on))
    {
        *rendering = rendering_on;
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(location.w - 300);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.00f, 1.00f, 0.00f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.85f, 0.85f, 1.f));

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.f));
    if (ImGui::Button("##Capture", ImVec2(140, 50)))
    {
        *cap = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Click button to save images");

    ImGui::PopStyleColor();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    std::stringstream msg_img;
    msg_img << "Images captured: " << num_images;
    ImGui::Text(msg_img.str().c_str());


    ImGui::End();
}
