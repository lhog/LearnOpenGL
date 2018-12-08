#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <unordered_map>
#include <vector>
#include <tuple>

class Shader
{
public:
    unsigned int programId;

	struct ShaderArgsValue {
		const char* path;
		GLuint stage;
		std::vector<std::string>& definitions;
	};

	typedef std::vector<ShaderArgsValue> ShaderArguments;
    // constructor generates the shader on the fly
    // ------------------------------------------------------------------------
	Shader() = delete;
	Shader(const ShaderArguments& shaderArgs) {
		initShader(shaderArgs);
	}
	Shader(const char* csPath, std::vector<std::string>& definitions = std::vector<std::string>()) {
		ShaderArguments shaderArgs;

		shaderArgs.emplace_back(
			ShaderArgsValue{
				csPath,
				GL_COMPUTE_SHADER,
				definitions
			}
		);

		initShader(shaderArgs);
	}
	Shader(const char* vsPath, const char* fsPath, std::vector<std::string>& definitions = std::vector<std::string>()) {
		ShaderArguments shaderArgs;

		shaderArgs.emplace_back(
			ShaderArgsValue{
				vsPath,
				GL_VERTEX_SHADER,
				definitions
			}
		);

		shaderArgs.emplace_back(
			ShaderArgsValue{
				fsPath,
				GL_FRAGMENT_SHADER,
				definitions
			}
		);

		initShader(shaderArgs);
    }
    // activate the shader
    // ------------------------------------------------------------------------
    void use() { 
        glUseProgram(programId);
    }
    // utility uniform functions
    // ------------------------------------------------------------------------
    void setBool(const std::string &name, bool value) const {         
        glUniform1i(glGetUniformLocation(programId, name.c_str()), (int)value);
    }
    // ------------------------------------------------------------------------
    void setInt(const std::string &name, int value) const { 
        glUniform1i(glGetUniformLocation(programId, name.c_str()), value);
    }
    // ------------------------------------------------------------------------
    void setFloat(const std::string &name, float value) const { 
        glUniform1f(glGetUniformLocation(programId, name.c_str()), value);
    }

private:
    // utility function for checking shader compilation/linking errors.
    // ------------------------------------------------------------------------
	GLuint LoadAndCompile(const ShaderArgsValue& shArg) {
		// 1. retrieve the source code from cpath
		std::string code;
		std::ifstream shaderFile;
		// ensure ifstream objects can throw exceptions:
		shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		try
		{
			// open file
			shaderFile.open(shArg.path);
			std::stringstream shaderStream;
			for (const auto& def : shArg.definitions) {
				shaderStream << def << std::endl;
			}
			// read file's buffer contents into stream
			shaderStream << shaderFile.rdbuf();
			// close file handler
			shaderFile.close();
			// convert stream into string
			code = shaderStream.str();
		}
		catch (std::ifstream::failure e)
		{
			std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
		}

		// 2. compile shaders
		unsigned int shader;

		shader = glCreateShader(shArg.stage);
		const char* ccode = code.c_str();
		glShaderSource(shader, 1, &ccode, NULL);
		glCompileShader(shader);
		checkCompileErrors(shader, shArg.stage);
		return shader;
	}

	void initShader(const ShaderArguments& shaderArgs) {
		programId = glCreateProgram();
		
		std::vector<GLuint> shaderObjs;

		for (const auto& shArg : shaderArgs) {
			GLuint shaderObj = LoadAndCompile(shArg);
			glAttachShader(programId, shaderObj);
			shaderObjs.emplace_back(shaderObj);
		}
		
		glLinkProgram(programId);
		checkCompileErrors(programId, 0);

		// delete the shaders as they're linked into our program now and no longer necessary
		for (const auto& shaderObj : shaderObjs) {
			glDeleteShader(shaderObj);
		}
	}

	const std::unordered_map<GLenum, const std::string> typeToString = {
		{ 0, "PROGRAM" },
		{ GL_VERTEX_SHADER, "GL_VERTEX_SHADER" },
		{ GL_FRAGMENT_SHADER, "GL_FRAGMENT_SHADER" },
		{ GL_TESS_CONTROL_SHADER, "GL_TESS_CONTROL_SHADER" },
		{ GL_TESS_EVALUATION_SHADER, "GL_TESS_EVALUATION_SHADER" },
		{ GL_GEOMETRY_SHADER, "GL_GEOMETRY_SHADER" },
		{ GL_COMPUTE_SHADER, "GL_COMPUTE_SHADER" }
	};

    void checkCompileErrors(unsigned int shader, const GLenum checkType) {
        int success;        
        if (checkType != 0)
        {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
				GLint len = -1;
				char *infoLog;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
				if (len > 0) {
					infoLog = static_cast<char*>(std::malloc(len * sizeof(char)));
					glGetShaderInfoLog(shader, len, NULL, infoLog);
					std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << typeToString.at(checkType) << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
					std::free(infoLog);
				}
            }
        }
        else
        {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success)
            {
				GLint len = -1;
				char *infoLog;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
				if (len > 0) {
					infoLog = static_cast<char*>(std::malloc(len * sizeof(char)));
					glGetProgramInfoLog(shader, len, NULL, infoLog);
					std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << typeToString.at(checkType) << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
					std::free(infoLog);
				}
            }
        }
    }
};
#endif