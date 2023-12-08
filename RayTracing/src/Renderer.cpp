#include "Renderer.h"
#include "Walnut/Random.h"

namespace Utils {
	static uint32_t ConvertToABGR(const glm::vec4& color) {
		uint8_t r = (uint8_t)(color.r * 255.0f);
		uint8_t g = (uint8_t)(color.g * 255.0f);
		uint8_t b = (uint8_t)(color.b * 255.0f);
		uint8_t a = (uint8_t)(color.a * 255.0f);

		uint32_t result = (a << 24) | (b << 16) | (g << 8) | r;
		return result;
	}
}

void Renderer::Render(const Camera& camera) {
	Ray ray;
	ray.Origin = camera.GetPosition();

	for (uint32_t y = 0; y < m_Image->GetHeight(); y++) {
		for (uint32_t x = 0; x < m_Image->GetWidth(); x++) {
			ray.Direction = camera.GetRayDirections()[x + y * m_Image->GetWidth()];

			glm::vec4 color = TraceRay(ray);
			color = glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y * m_Image->GetWidth()] = Utils::ConvertToABGR(color);
		}
	}

	m_Image->SetData(m_ImageData);
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (m_Image) {
		if (m_Image->GetWidth() == width && m_Image->GetHeight() == height)
			return;

		m_Image->Resize(width, height);
	}
	else {
		m_Image = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);
	}

	delete[] m_ImageData;
	m_ImageData = new uint32_t[width * height];
}

glm::vec4 Renderer::TraceRay(const Ray& ray) {
	float radius = 0.5f;
	glm::vec3 circleOrigin(-0.5f,0,0);
	// (b_x^2 + b_y^2 + b_z^2)t^2 + (2a_xb_x - 2ab_x + 2a_y b_y - 2bb_y + 2a_zb_z - 2cb_z)t + (a_x^2 + a^2 - 2a·a_x + a_y^2 + b^2 - 2b·a_y + a_z^2 + c^2 - 2c·a_z - r^2)

	glm::vec3 origin = ray.Origin - glm::vec3(-0.5f, 0.0f, 0.0f);

	float a = glm::dot(ray.Direction, ray.Direction);
	float b = 2.0f * glm::dot(ray.Origin, ray.Direction) - 2.0f * glm::dot(circleOrigin, ray.Direction); //-2.0f*glm::dot(circleOrigin,ray.Direction)
	float c = glm::dot(ray.Origin, ray.Origin) - radius * radius + glm::dot(circleOrigin, circleOrigin) - 2.0f * glm::dot(circleOrigin, ray.Origin);
	//+ glm::dot(circleOrigin, circleOrigin) - 2.0f * glm::dot(circleOrigin, ray.Origin)

	float d = b*b - 4.0f * a * c;

	if (d < 0.0f)
		return glm::vec4(0, 0, 0, 1);

	float t0 = (-b + glm::sqrt(d)) / (2.0f * a);
	float tClosest = (-b - glm::sqrt(d)) / (2.0f * a);

	glm::vec3 hitPoint = ray.Origin + ray.Direction * tClosest;
	glm::vec3 normal = glm::normalize(hitPoint);

	glm::vec3 lightDir = glm::normalize(glm::vec3(-1, -1, -1));

	float dotProduct = glm::max(glm::dot(normal, -lightDir), 0.0f);

	glm::vec3 sphereColor(0.5f, 0, 1);
	sphereColor *= dotProduct;
	return glm::vec4(sphereColor, 1.0f);
}
