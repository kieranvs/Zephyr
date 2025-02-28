#pragma once

#include "Shader.hpp"
#include "Types.hpp"

namespace ECS
{
	class Storage;
}
namespace System
{
	class Scene;
}
namespace OpenGL
{
	class PhongRenderer
	{
		Shader m_phong_shader; // Used to fetch the Directional, Point and spot light buffers.

		Utility::ResourceRef<SSBO> m_directional_lights_buffer; // The SSBO used across shaders to bind DirectionalLight data.
		GLsizeiptr m_directional_light_fixed_size; // Size in bytes of the fixed portion of the directional light shader storage block (excludes any variable-sized-array variables sizes).
		GLsizeiptr m_directional_light_count_offset;
		GLint m_directional_light_array_stride;
		GLsizeiptr m_directional_light_array_start_offset;
		GLsizeiptr m_directional_light_direction_offset;
		GLsizeiptr m_directional_light_ambient_offset;
		GLsizeiptr m_directional_light_diffuse_offset;
		GLsizeiptr m_directional_light_specular_offset;

		Utility::ResourceRef<SSBO> m_point_lights_buffer; // The SSBO used across shaders to bind PointLight data.
		GLsizeiptr m_point_light_fixed_size; // Size in bytes of the fixed portion of the point light shader storage block (excludes any variable-sized-array variables sizes).
		GLsizeiptr m_point_light_count_offset;
		GLint m_point_light_array_stride;
		GLsizeiptr m_point_light_array_start_offset;
		GLsizeiptr m_point_light_position_offset;
		GLsizeiptr m_point_light_constant_offset;
		GLsizeiptr m_point_light_linear_offset;
		GLsizeiptr m_point_light_quadratic_offset;
		GLsizeiptr m_point_light_ambient_offset;
		GLsizeiptr m_point_light_diffuse_offset;
		GLsizeiptr m_point_light_specular_offset;

		Utility::ResourceRef<SSBO> m_spot_lights_buffer; // The SSBO used across shaders to bind SpotLight data.
		GLsizeiptr m_spot_light_fixed_size; // Size in bytes of the fixed portion of the spot light shader storage block (excludes any variable-sized-array variables sizes).
		GLsizeiptr m_spot_light_count_offset;
		GLint m_spot_light_array_stride;
		GLsizeiptr m_spot_light_array_start_offset;
		GLsizeiptr m_spot_light_position_offset;
		GLsizeiptr m_spot_light_direction_offset;
		GLsizeiptr m_spot_light_cutoff_offset;
		GLsizeiptr m_spot_light_outer_cutoff_offset;
		GLsizeiptr m_spot_light_constant_offset;
		GLsizeiptr m_spot_light_linear_offset;
		GLsizeiptr m_spot_light_quadratic_offset;
		GLsizeiptr m_spot_light_ambient_offset;
		GLsizeiptr m_spot_light_diffuse_offset;
		GLsizeiptr m_spot_light_specular_offset;

	public:
		PhongRenderer();
		Shader& get_shader() { return m_phong_shader; }

		// Update the storage block buffer object data for all the lights in p_storage.
		// Only needs to happen once per frame or on changes to a light.
		void update_light_data(System::Scene& p_scene, const Texture& p_shadow_map);

	};
} // namespace OpenGL