#ifndef DEBUGCALLBACK_H
#define DEBUGCALLBACK_H

#include <GLFW/glfw3.h>
#include <string>

using namespace std;

namespace LearnOpenGL {
	void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id,
		GLenum severity, GLsizei length, const GLchar * msg, const void * param ) {
		
		string sourceStr;
		switch(source) {
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
			sourceStr = "WindowSys";
			break;
		case GL_DEBUG_SOURCE_APPLICATION:
			sourceStr = "App";
			break;
		case GL_DEBUG_SOURCE_API:
			sourceStr = "OpenGL";
			break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER:
			sourceStr = "ShaderCompiler";
			break;
		case GL_DEBUG_SOURCE_THIRD_PARTY:
			sourceStr = "3rdParty";
			break;
		case GL_DEBUG_SOURCE_OTHER:
			sourceStr = "Other";
			break;
		default:
			sourceStr = "Unknown";
		}
		
		string typeStr;
		switch(type) {
		case GL_DEBUG_TYPE_ERROR:
			typeStr = "Error";
			break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
			typeStr = "Deprecated";
			break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
			typeStr = "Undefined";
			break;
		case GL_DEBUG_TYPE_PORTABILITY:
			typeStr = "Portability";
			break;
		case GL_DEBUG_TYPE_PERFORMANCE:
			typeStr = "Performance";
			break;
		case GL_DEBUG_TYPE_MARKER:
			typeStr = "Marker";
			break;
		case GL_DEBUG_TYPE_PUSH_GROUP:
			typeStr = "PushGrp";
			break;
		case GL_DEBUG_TYPE_POP_GROUP:
			typeStr = "PopGrp";
			break;
		case GL_DEBUG_TYPE_OTHER:
			typeStr = "Other";
			break;
		default:
			typeStr = "Unknown";
		}
		
		string sevStr;
		switch(severity) {
		case GL_DEBUG_SEVERITY_HIGH:
			sevStr = "HIGH";
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			sevStr = "MED";
			break;
		case GL_DEBUG_SEVERITY_LOW:
			sevStr = "LOW";
			break;
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			sevStr = "NOTIFY";
			break;
		default:
			sevStr = "UNK";
		}
		
		printf("%s:%s[%s](%d): %s\n", sourceStr.c_str(), typeStr.c_str(), sevStr.c_str(), 
			id, msg);		
	}
}	
#endif