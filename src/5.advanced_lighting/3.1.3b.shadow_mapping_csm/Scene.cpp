#include <map>
#include <algorithm>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Scene.h"


class BoundingSphere;
class BoundingSphere {
private:
	glm::vec3 center;
	float radius;
	int count;

	void UpdateCenterAndRadius(const glm::vec3& newCenter, const float newRadius) {
		//calculate new average center
		center = static_cast<float>(count) * center;
		center += newCenter;
		center /= ++count;

		//calculate maximum radius
		float dist = glm::distance(center, newCenter);
		radius = std::max(radius, dist + newRadius);
	}
public:
	BoundingSphere() :
		center(glm::vec3(0.0)), radius(0.0f), count(0) {};

	BoundingSphere(const std::vector<float>& arr, const int strideSize, const int argOffset, const glm::mat4& sceneMatrix) : BoundingSphere() {
		const int stridesNum = arr.size() / strideSize;
		for (int i = 0; i < stridesNum; ++i) {
			glm::vec4 modelVertex = glm::vec4(
				static_cast<float>(arr[i * strideSize + argOffset + 0]), //x
				static_cast<float>(arr[i * strideSize + argOffset + 1]), //y
				static_cast<float>(arr[i * strideSize + argOffset + 2]), //z
				1.0f													 //w
			);
			glm::vec4 worldVertex = sceneMatrix * modelVertex;
			Add(worldVertex);
		}
	}

	void Clean() {
		center = glm::vec3(0.0);
		radius = 0.0;
		count = 0;
	}

	void Add(const glm::vec4& vertex) {
		UpdateCenterAndRadius(glm::vec3(vertex), 0.0f);
	}

	void Add(const glm::vec3& vertex) {
		UpdateCenterAndRadius(vertex, 0.0f);
	}

	void Add(const BoundingSphere& bs) {
		const glm::vec3 bsCenter = bs.GetCenter();
		const float bsRadius = bs.GetRadius();
		UpdateCenterAndRadius(bsCenter, bsRadius);
		count += bs.GetCount();
	}

	const glm::vec3& GetCenter() const { return center; }
	const float GetRadius() const { return radius; }

	const int GetCount() const { return count; }

	void GetMinMax(glm::vec4& min, glm::vec4& max, const glm::mat4& transformMatrix) const {
		glm::vec4 tCenter = transformMatrix * glm::vec4(center, 1.0f);
		min = tCenter - radius;
		max = tCenter + radius;
	}
};

class VertexAttribute {
private:
	const int argSize;
	const int argOffset;
	const GLboolean normalized;
public:
	void EnableAndDeclare(const GLuint index, const int strideSize) {
		glEnableVertexAttribArray(index);
		glVertexAttribPointer(index, argSize, GL_FLOAT, normalized, strideSize * sizeof(float), (void*)(argOffset * sizeof(float)));
	}
	VertexAttribute() = delete;
	//VertexAttribute(VertexAttribute&) = delete;
	VertexAttribute(const int argSize, const int argOffset, const GLboolean normalized) : argSize(argSize), argOffset(argOffset), normalized(normalized) {};

	const int GetArgSize() const { return argSize; }
	const int GetArgOffset() const { return argOffset; }
};

class ModelObject {
private:
	const std::vector<float> attributeArray;
	std::vector<VertexAttribute> attributeProperties;
	const int strideSize;
	GLuint vao;
	GLuint vbo;
public:
	ModelObject() = delete;
	ModelObject(const std::vector<float>& attributeArray, std::vector<VertexAttribute>& attributeProperties, const int strideSize) :
		attributeArray(attributeArray), attributeProperties(attributeProperties), strideSize(strideSize)
	{
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * attributeArray.size(), &attributeArray[0], GL_STATIC_DRAW);

		GLuint attrId = 0;
		for (auto& ap : attributeProperties) {
			ap.EnableAndDeclare(attrId, strideSize);
			++attrId;
		}

		glBindVertexArray(0);
	}
	~ModelObject() {
		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(1, &vbo);
	}
	const std::vector<float>&  GetAttributeArray() const { return attributeArray; }
	const int GetStrideSize() const { return strideSize; }
	const GLuint GetVAO() const { return vao; }
	const GLuint GetVBO() const { return vbo; }
	void Render() const {
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, attributeArray.size() / strideSize);
	}
};

class SceneObject {
private:
	const ModelObject& modelObject;
	const BoundingSphere boundingSphere;
	glm::mat4 sceneMatrix;
public:
	SceneObject(const ModelObject& modelObject, const VertexAttribute& positionAttribute, glm::mat4 sceneMatrix) : modelObject(modelObject), sceneMatrix(std::move(sceneMatrix)),
		boundingSphere(BoundingSphere(modelObject.GetAttributeArray(), modelObject.GetStrideSize(), positionAttribute.GetArgOffset(), sceneMatrix)) {};
	SceneObject(ModelObject& modelObject, const VertexAttribute& positionAttribute) : SceneObject(modelObject, positionAttribute, glm::mat4()) {};
	const BoundingSphere& GetBoundingSphere() const { return boundingSphere; }
	const glm::mat4& GetSceneMatrix() const { return sceneMatrix; }
	void Render() const {
		modelObject.Render();
	}
};

class Scene {
private:
	BoundingSphere bs;
	std::map<const int, const SceneObject&> sceneObjects;
public:
	void Add(const int id, const SceneObject& so, const bool calcBoundingSphere = true) {
		if (calcBoundingSphere) bs.Add(so.GetBoundingSphere());
		sceneObjects.emplace(id, so);
	}
	void Delete(const int id) {
		sceneObjects.erase(id);
		bs.Clean();
		for (auto& soPair : sceneObjects) {
			bs.Add(soPair.second.GetBoundingSphere());
		}
	}
	const SceneObject& Get(const int id) {
		return sceneObjects.at(id);
	}

	const BoundingSphere& GetBoundingSphere() const { return bs; }
	template<typename Functor>
	void Render(const Functor& func) const {
		for (auto& soPair : sceneObjects) {
			func(soPair.first, soPair.second);
			soPair.second.Render();
		}
		glBindVertexArray(0);
	}
};