#include "Editor.hpp"

#include "ECS/Storage.hpp"
#include "Component/Camera.hpp"
#include "Component/Collider.hpp"
#include "Component/Label.hpp"
#include "Component/Lights.hpp"
#include "Component/Mesh.hpp"
#include "Component/RigidBody.hpp"
#include "Component/Terrain.hpp"
#include "Component/Texture.hpp"
#include "Component/Transform.hpp"
#include "System/CollisionSystem.hpp"
#include "System/MeshSystem.hpp"
#include "System/SceneSystem.hpp"
#include "System/TextureSystem.hpp"

#include "OpenGL/DebugRenderer.hpp"
#include "OpenGL/OpenGLRenderer.hpp"
#include "Platform/Core.hpp"
#include "Platform/Input.hpp"
#include "Platform/Window.hpp"
#include "Utility/Logger.hpp"
#include "Utility/Utility.hpp"

#include "imgui.h"
#include "ImGuizmo.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <format>

namespace UI
{
	Editor::Editor(Platform::Input& p_input, Platform::Window& p_window, System::TextureSystem& p_texture_system, System::MeshSystem& p_mesh_system, System::SceneSystem& p_scene_system, System::CollisionSystem& p_collision_system, OpenGL::OpenGLRenderer& p_openGL_renderer)
		: m_input{p_input}
		, m_window{p_window}
		, m_texture_system{p_texture_system}
		, m_mesh_system{p_mesh_system}
		, m_scene_system{p_scene_system}
		, m_collision_system{p_collision_system}
		, m_openGL_renderer{p_openGL_renderer}
		, m_click_rays{}
		, m_selected_entities{}
		, m_console{}
		, m_windows_to_display{}
		, m_draw_count{0}
		, m_time_to_average_over{std::chrono::seconds(1)}
		, m_duration_between_draws{}
	{
		m_input.m_mouse_event.subscribe(this, &Editor::on_mouse_event);
		m_input.m_key_event.subscribe(this,   &Editor::on_key_event);

		initialiseStyling();
	}

	void Editor::on_mouse_event(Platform::MouseButton p_button, Platform::Action p_action)
	{
		if (p_button == Platform::MouseButton::Right && p_action == Platform::Action::Press)
		{
			if (m_input.cursor_captured())
				m_input.set_cursor_mode(Platform::CursorMode::Normal);
			else if (!m_input.cursor_over_UI()) // We are editing. If we click on non-UI re-capture mouse
				m_input.set_cursor_mode(Platform::CursorMode::Captured);
		}
		if (m_input.cursor_captured())
			return;

		if (!m_input.cursor_over_UI())
		{
			switch (p_button)
			{
				case Platform::MouseButton::Left:
				{
					if (p_action == Platform::Action::Press)
					{
						const auto& view_info = m_openGL_renderer.m_view_information;
						auto cursorRay = Utility::get_cursor_ray(m_input.cursor_position(), m_window.size(), view_info.m_view_position, view_info.m_projection, view_info.m_view);
						m_click_rays.emplace_back(cursorRay);
						auto entitiesUnderMouse = m_collision_system.get_entities_along_ray(cursorRay);

						if (!entitiesUnderMouse.empty())
						{
							std::sort(entitiesUnderMouse.begin(), entitiesUnderMouse.end(), [](const auto& left, const auto& right) { return left.second < right.second; });
							auto entityCollided = entitiesUnderMouse.front().first;

							m_selected_entities.push_back(entityCollided);
							LOG("[EDITOR] Entity{} has been selected", entityCollided.ID);
						}
						else
						{
							m_selected_entities.clear();
							LOG("[EDITOR] Deselected entities");
						}
					}
					break;
				}
				case Platform::MouseButton::Middle:
				{
					m_click_rays.clear();
					break;
				}
				case Platform::MouseButton::Right: break;
				default: break;
			}
		}
	}
	void Editor::on_key_event(Platform::Key p_key, Platform::Action p_action)
	{ (void)p_key; (void)p_action; }

	void Editor::draw(const DeltaTime& p_duration_since_last_draw)
	{
		m_duration_between_draws.push_back(p_duration_since_last_draw);

		for (const auto& ray : m_click_rays)
			OpenGL::DebugRenderer::add(ray);

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("View"))
			{
				ImGui::MenuItem("Entity hierarchy", NULL, &m_windows_to_display.Entity);
				ImGui::MenuItem("Console",          NULL, &m_windows_to_display.Console);

				if (ImGui::BeginMenu("Debug"))
				{
					ImGui::MenuItem("Debug options", NULL, &m_windows_to_display.Debug);
					ImGui::MenuItem("FPS Timer",     NULL, &m_windows_to_display.FPSTimer);
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("ImGui"))
				{
					ImGui::MenuItem("Demo",             NULL, &m_windows_to_display.ImGuiDemo);
					ImGui::MenuItem("Metrics/Debugger", NULL, &m_windows_to_display.ImGuiMetrics);
					ImGui::MenuItem("Stack",            NULL, &m_windows_to_display.ImGuiStack);
					ImGui::MenuItem("About",            NULL, &m_windows_to_display.ImGuiAbout);
					ImGui::MenuItem("Style Editor",     NULL, &m_windows_to_display.ImGuiStyleEditor);

					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}
			if (m_windows_to_display.FPSTimer)
			{
				auto fps = get_fps(m_duration_between_draws, m_time_to_average_over);
				std::string fps_str = std::format("FPS: {:.1f}", fps);
				ImGui::SameLine((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(fps_str.c_str()).x - ImGui::GetStyle().ItemSpacing.x) / 2.f);

				glm::vec4 colour;
				if      (fps > 60.f) colour = glm::vec4(0.f, 255.f, 0.f, 255.f);
				else if (fps > 30.f) colour = glm::vec4(255.f, 255.f, 0.f, 255.f);
				else                 colour = glm::vec4(255.f, 0.f, 0.f, 255.f);

				ImGui::TextColored(colour, "%s", fps_str.c_str());
			}
			ImGui::EndMenuBar();
		}
		if (m_windows_to_display.Entity)        draw_entity_tree_window();
		if (m_windows_to_display.Console)       draw_console_window();
		draw_debug_window();
		if (m_windows_to_display.ImGuiDemo)     ImGui::ShowDemoWindow(&m_windows_to_display.ImGuiDemo);
		if (m_windows_to_display.ImGuiMetrics)  ImGui::ShowMetricsWindow(&m_windows_to_display.ImGuiMetrics);
		if (m_windows_to_display.ImGuiStack)    ImGui::ShowStackToolWindow(&m_windows_to_display.ImGuiStack);
		if (m_windows_to_display.ImGuiAbout)    ImGui::ShowAboutWindow(&m_windows_to_display.ImGuiAbout);
		if (m_windows_to_display.ImGuiStyleEditor)
		{
			ImGui::Begin("Dear ImGui Style Editor", &m_windows_to_display.ImGuiStyleEditor);
			ImGui::ShowStyleEditor();
			ImGui::End();
		}

		{// Manipulators
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();
			auto window_pos  = ImGui::GetWindowPos();
			auto window_size = ImGui::GetWindowSize();
			ImGuizmo::SetRect(window_pos.x, window_pos.y, window_size.x, window_size.y);

			for (const auto& selected_ent : m_selected_entities)
			{
				if (m_scene_system.get_current_scene().has_components<Component::Transform>(selected_ent))
				{
					auto& transform = m_scene_system.get_current_scene().get_component<Component::Transform>(selected_ent);

					ImGuizmo::Manipulate(
						glm::value_ptr(m_openGL_renderer.m_view_information.m_view),
						glm::value_ptr(m_openGL_renderer.m_view_information.m_projection),
						ImGuizmo::OPERATION::UNIVERSAL,
						ImGuizmo::LOCAL,
						glm::value_ptr(transform.m_model));

					if (ImGuizmo::IsUsing())
						transform.set_model_matrix(transform.m_model);

					break; // ImGuizmo only allows one entity to be edited at a time.
				}
			}
		}

		m_draw_count++;
	}
	void Editor::draw_entity_tree_window()
	{
		if (ImGui::Begin("Entities", &m_windows_to_display.Entity))
		{
			auto& scene = m_scene_system.get_current_scene();
			scene.foreach([&](ECS::Entity& p_entity)
			{
				std::string title = "Entity " + std::to_string(p_entity.ID);
				if (scene.has_components<Component::Label>(p_entity))
				{
					auto label = scene.get_component<Component::Label&>(p_entity);
					title = label.mName;
				}

				if (ImGui::TreeNode(title.c_str()))
				{
					if (scene.has_components<Component::Transform>(p_entity))
						scene.get_component<Component::Transform&>(p_entity).draw_UI();
					if (scene.has_components<Component::Collider>(p_entity))
						scene.get_component<Component::Collider&>(p_entity).draw_UI();
					if (scene.has_components<Component::RigidBody>(p_entity))
						scene.get_component<Component::RigidBody&>(p_entity).draw_UI();
					if (scene.has_components<Component::DirectionalLight>(p_entity))
						scene.get_component<Component::DirectionalLight&>(p_entity).draw_UI();
					if (scene.has_components<Component::SpotLight>(p_entity))
						scene.get_component<Component::SpotLight&>(p_entity).draw_UI();
					if (scene.has_components<Component::PointLight>(p_entity))
						scene.get_component<Component::PointLight&>(p_entity).draw_UI();
					if (scene.has_components<Component::Camera>(p_entity))
						scene.get_component<Component::Camera>(p_entity).draw_UI();
					if (scene.has_components<Component::ParticleEmitter>(p_entity))
						scene.get_component<Component::ParticleEmitter>(p_entity).draw_UI(m_texture_system);
					if (scene.has_components<Component::Terrain>(p_entity))
						scene.get_component<Component::Terrain>(p_entity).draw_UI(m_texture_system);
					if (scene.has_components<Component::Mesh>(p_entity))
						scene.get_component<Component::Mesh>(p_entity).draw_UI();
					if (scene.has_components<Component::Texture>(p_entity))
						scene.get_component<Component::Texture>(p_entity).draw_UI(m_texture_system);

					ImGui::SeparatorText("Quick options");
					if (ImGui::Button("Delete entity"))
						scene.delete_entity(p_entity);

					ImGui::Separator();
					ImGui::TreePop();
				}
			});
		}
		ImGui::End();
	}
	void Editor::draw_console_window()
	{
		m_console.draw("Console", &m_windows_to_display.Console);
	}
	void Editor::draw_debug_window()
	{
		if (m_windows_to_display.Debug)
		{
			if (ImGui::Begin("Debug options", &m_windows_to_display.Debug))
			{
				{ ImGui::SeparatorText("Graphics");
					ImGui::Text("Window size", m_window.size());
					ImGui::Text("Aspect ratio", m_window.aspect_ratio());
					bool VSync = m_window.get_VSync();
					ImGui::Text("View Position", m_openGL_renderer.m_view_information.m_view_position);
					ImGui::Separator();
					ImGui::Checkbox("Show light positions", &OpenGL::DebugRenderer::m_debug_options.m_show_light_positions);
					ImGui::Checkbox("Visualise normals", &OpenGL::DebugRenderer::m_debug_options.m_show_mesh_normals);
					if (ImGui::Checkbox("VSync", &VSync))
						m_window.set_VSync(VSync);
				}

				{ ImGui::SeparatorText("Post Processing");
					ImGui::Checkbox("Invert", &m_openGL_renderer.m_post_processing_options.mInvertColours);
					ImGui::Checkbox("Grayscale", &m_openGL_renderer.m_post_processing_options.mGrayScale);
					ImGui::Checkbox("Sharpen", &m_openGL_renderer.m_post_processing_options.mSharpen);
					ImGui::Checkbox("Blur", &m_openGL_renderer.m_post_processing_options.mBlur);
					ImGui::Checkbox("Edge detection", &m_openGL_renderer.m_post_processing_options.mEdgeDetection);

					const bool isPostProcessingOn = m_openGL_renderer.m_post_processing_options.mInvertColours
						|| m_openGL_renderer.m_post_processing_options.mGrayScale || m_openGL_renderer.m_post_processing_options.mSharpen
						|| m_openGL_renderer.m_post_processing_options.mBlur      || m_openGL_renderer.m_post_processing_options.mEdgeDetection;

					if (!isPostProcessingOn) ImGui::BeginDisabled();
						ImGui::SliderFloat("Kernel offset", &m_openGL_renderer.m_post_processing_options.mKernelOffset, -1.f, 1.f);
					if (!isPostProcessingOn) ImGui::EndDisabled();
				}

				{ImGui::SeparatorText("Physics");
					ImGui::Checkbox("Show orientations",              &OpenGL::DebugRenderer::m_debug_options.m_show_orientations);
					ImGui::Checkbox("Show bounding box",              &OpenGL::DebugRenderer::m_debug_options.m_show_bounding_box);

					if (!OpenGL::DebugRenderer::m_debug_options.m_show_bounding_box) ImGui::BeginDisabled();
					ImGui::Checkbox("Fill bounding box",              &OpenGL::DebugRenderer::m_debug_options.m_fill_bounding_box);
					ImGui::ColorEdit3("Bounding box colour",          &OpenGL::DebugRenderer::m_debug_options.m_bounding_box_colour[0]);
					ImGui::ColorEdit3("Bounding box collided colour", &OpenGL::DebugRenderer::m_debug_options.m_bounding_box_collided_colour[0]);
					if (!OpenGL::DebugRenderer::m_debug_options.m_show_bounding_box) ImGui::EndDisabled();

					ImGui::Checkbox("Show collision shape",           &OpenGL::DebugRenderer::m_debug_options.m_show_collision_shape);
					ImGui::Slider("Position offset factor",            OpenGL::DebugRenderer::m_debug_options.m_position_offset_factor, -10.f, 10.f);
					ImGui::Slider("Position offset units",             OpenGL::DebugRenderer::m_debug_options.m_position_offset_units, -10.f, 10.f);
				}

				if (ImGui::Button("Reset"))
					OpenGL::DebugRenderer::m_debug_options = OpenGL::DebugRenderer::DebugOptions();
			}
			ImGui::End();
		}
	}

	void Editor::log(const std::string& p_message)
	{
		m_console.add_log({p_message});
	}
	void Editor::log_warning(const std::string& p_message)
	{
		m_console.add_log({p_message, glm::vec3(1.f, 1.f, 0.f)});
	}
	void Editor::log_error(const std::string& p_message)
	{
		m_console.add_log({p_message, glm::vec3(1.f, 0.f, 0.f)});
	}
	void Editor::initialiseStyling()
	{
		ImGui::StyleColorsDark();

		// Round out the UI and make more compact
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowPadding            = ImVec2(4.f, 2.f);
		style.FramePadding             = ImVec2(4.f, 2.f);
		style.CellPadding              = ImVec2(4.f, 0.f);
		style.ItemSpacing              = ImVec2(4.f, 3.f);
		style.ItemInnerSpacing         = ImVec2(4.f, 2.f);
		style.TouchExtraPadding        = ImVec2(0.f, 0.f);
		style.IndentSpacing            = 16.f;
		style.ScrollbarSize            = 10.f;
		style.GrabMinSize              = 10.f;

		style.WindowBorderSize         = 1.f;
		style.ChildBorderSize          = 1.f;
		style.PopupBorderSize          = 1.f;
		style.FrameBorderSize          = 0.f;
		style.TabBorderSize            = 0.f;

		style.WindowRounding           = 4.f;
		style.ChildRounding            = 4.f;
		style.FrameRounding            = 4.f;
		style.PopupRounding            = 4.f;
		style.ScrollbarRounding        = 4.f;
		style.GrabRounding             = 4.f;
		style.LogSliderDeadzone        = 4.f;
		style.TabRounding              = 4.f;

		style.WindowTitleAlign         = ImVec2(0.5f, 0.5f);
		style.WindowMenuButtonPosition = ImGuiDir_Right;
		style.ColorButtonPosition      = ImGuiDir_Right;
		style.ButtonTextAlign          = ImVec2(0.5f, 0.5f);
		style.SelectableTextAlign      = ImVec2(0.5f, 0.5f);
		style.DisplaySafeAreaPadding   = ImVec2(0.f, 0.f);

		auto theme_grey = ImVec4(0.174f, 0.174f, 0.174f, 1.000f);
		style.Colors[ImGuiCol_MenuBarBg] = theme_grey;
	}
} // namespace UI