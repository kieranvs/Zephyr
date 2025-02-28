#include "Shader.hpp"

#include "Utility/Utility.hpp"
#include "Utility/File.hpp"
#include "Utility/Config.hpp"
#include "Component/Vertex.hpp"

#include "glad/glad.h" // #TODO: Implement missing functions into the GLState and remove this include

#include "glm/gtc/type_ptr.hpp"

#include <algorithm>
#include <sstream>

namespace OpenGL
{
	Shader::Shader(const char* p_name)
		: m_name{p_name}
		, m_handle{0}
		, m_vertex_attributes{}
		, m_uniform_blocks{}
		, m_shader_storage_blocks{}
		, m_uniforms{}
	{
		const auto shaderPath = Config::GLSL_Shader_Directory / p_name;

		unsigned int vertexShader;
		auto vertexShaderPath = shaderPath;
		vertexShaderPath.replace_extension("vert");
		ASSERT(Utility::File::exists(vertexShaderPath), "[OPENGL][SHADER] Vertex shader does not exist at path '{}'", vertexShaderPath.string());
		std::string vertexSource = Utility::File::read_from_file(vertexShaderPath);
		{
			vertexShader = create_shader(ShaderProgramType::Vertex);
			shader_source(vertexShader, vertexSource);
			compile_shader(vertexShader);
			scanForAttributes(vertexSource);
		}

		unsigned int fragmentShader;
		{
			auto fragmentShaderPath = shaderPath;
			fragmentShaderPath.replace_extension("frag");
			ASSERT(Utility::File::exists(fragmentShaderPath), "[OPENGL][SHADER] Fragment shader does not exist at path {}", fragmentShaderPath.string());
			fragmentShader     = create_shader(ShaderProgramType::Fragment);
			std::string source = Utility::File::read_from_file(fragmentShaderPath);
			shader_source(fragmentShader, source);
			compile_shader(fragmentShader);
		}

		std::optional<unsigned int> geometryShader;
		{
			auto geomShaderPath = shaderPath;
			geomShaderPath.replace_extension("geom");
			if (Utility::File::exists(shaderPath))
			{
				geometryShader     = create_shader(ShaderProgramType::Geometry);
				std::string source = Utility::File::read_from_file(shaderPath);
				shader_source(geometryShader.value(), source);
				compile_shader(geometryShader.value());
			}
		}

		{
			m_handle = create_program();
			attach_shader(m_handle, vertexShader);
			attach_shader(m_handle, fragmentShader);
			if (geometryShader.has_value())
				attach_shader(m_handle, geometryShader.value());

			link_program(m_handle);
		}

		{ // Setup loose uniforms (not belonging to uniform blocks or shader storage blocks)
			const GLint uniform_count = get_uniform_count(m_handle);

			for (int uniform_index = 0; uniform_index < uniform_count; ++uniform_index)
			{
				constexpr GLenum properties[1] = {GL_BLOCK_INDEX};
				GLint values[1];
				glGetProgramResourceiv(m_handle, GL_UNIFORM, uniform_index, 1, properties, 1, NULL, values);

				// If the variable is not the member of an interface block, the value is -1.
				if (values[0] == -1)
					m_uniforms.emplace_back(m_handle, uniform_index);
			}
		}
		{ // m_uniform_blocks setup
			const GLint block_count = get_uniform_block_count(m_handle);
			for (int block_index = 0; block_index < block_count; block_index++)
				m_uniform_blocks.emplace_back(m_handle, block_index);
		}
		{ // m_shader_storage_blocks
			const GLint block_count = get_shader_storage_block_count(m_handle);
			m_shader_storage_blocks.reserve(block_count);
			for (int block_index = 0; block_index < block_count; block_index++)
				m_shader_storage_blocks.emplace_back(m_handle, block_index);
		}

		// Delete the shaders after linking as they're no longer needed, they will be flagged for deletion,
		// but will not be deleted until they are no longer attached to any shader program object.
		delete_shader(vertexShader);
		delete_shader(fragmentShader);
		if (geometryShader.has_value())
			delete_shader(geometryShader.value());

		LOG("OpenGL::Shader '{}' loaded given ID: {}", m_name, m_handle);
	}

	void Shader::scanForAttributes(const std::string& p_source_code)
	{
		if (std::ranges::find(m_vertex_attributes, VertexAttribute::Position3D) == m_vertex_attributes.end())
			if (p_source_code.find(Data::get_identifier(Data::VertexAttribute::Position3D)) != std::string::npos)
				m_vertex_attributes.push_back(VertexAttribute::Position3D);

		if (std::ranges::find(m_vertex_attributes, VertexAttribute::Normal3D) == m_vertex_attributes.end())
			if (p_source_code.find(Data::get_identifier(Data::VertexAttribute::Normal3D)) != std::string::npos)
				m_vertex_attributes.push_back(VertexAttribute::Normal3D);

		if (std::ranges::find(m_vertex_attributes, VertexAttribute::ColourRGBA) == m_vertex_attributes.end())
			if (p_source_code.find(Data::get_identifier(Data::VertexAttribute::ColourRGBA)) != std::string::npos)
				m_vertex_attributes.push_back(VertexAttribute::ColourRGBA);

		if (std::ranges::find(m_vertex_attributes, VertexAttribute::TextureCoordinate2D) == m_vertex_attributes.end())
			if (p_source_code.find(Data::get_identifier(Data::VertexAttribute::TextureCoordinate2D)) != std::string::npos)
				m_vertex_attributes.push_back(VertexAttribute::TextureCoordinate2D);

		ASSERT(!m_vertex_attributes.empty(), "[OPENGL][SHADER] No Vertex attributes found in the shader.");
	}

	void Shader::use() const
	{
		use_program(m_handle);
	}

	Utility::ResourceRef<SSBO> Shader::get_SSBO_backing(const std::string& p_storage_block_identifier)
	{
		for (auto& storage_block : m_shader_storage_blocks)
		{
			if (storage_block.m_identifier == p_storage_block_identifier)
				return storage_block.m_buffer_backing;
		}
		return {};
	}

	void Shader::set_uniform(GLint p_location, bool p_value)
	{
		glUniform1i(p_location, (GLint)p_value); // Setting a boolean is treated as integer
	}
	void Shader::set_uniform(GLint p_location, int p_value)
	{
		glUniform1i(p_location, (GLint)p_value);
	}
	void Shader::set_uniform(GLint p_location, float p_value)
	{
		glUniform1f(p_location, p_value);
	}
	void Shader::set_uniform(GLint p_location, const glm::vec2& p_value)
	{
		glUniform2fv(p_location, 1, &p_value[0]);
	}
	void Shader::set_uniform(GLint p_location, const glm::vec3& p_value)
	{
		glUniform3fv(p_location, 1, &p_value[0]);
	}
	void Shader::set_uniform(GLint p_location, const glm::vec4& p_value)
	{
		glUniform4fv(p_location, 1, &p_value[0]);
	}
	void Shader::set_uniform(GLint p_location, const glm::mat2& p_value)
	{
		glUniformMatrix2fv(p_location, 1, GL_FALSE, &p_value[0][0]);
	}
	void Shader::set_uniform(GLint p_location, const glm::mat3& p_value)
	{
		glUniformMatrix3fv(p_location, 1, GL_FALSE, &p_value[0][0]);
	}
	void Shader::set_uniform(GLint p_location, const glm::mat4& p_value)
	{
		glUniformMatrix4fv(p_location, 1, GL_FALSE, glm::value_ptr(p_value));
	}
} // namespace OpenGL