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

void Renderer::Render(const Scene& scene, const Camera& camera) {
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;

	if (m_FrameIndex == 1)
		memset(m_AccumulationData, 0, m_Image->GetWidth() * m_Image->GetHeight() * sizeof(glm::vec4));
#define MT 1
#if MT
	std::for_each(std::execution::par, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(), [this](uint32_t y) {
		std::for_each(std::execution::par, m_ImageHorizontalIter.begin(), m_ImageHorizontalIter.end(), [this, y](uint32_t x) {
			glm::vec4 color = PerPixel(x, y);
			m_AccumulationData[x + y * m_Image->GetWidth()] += color;

			glm::vec4 accumulatedColor = m_AccumulationData[x + y * m_Image->GetWidth()];
			accumulatedColor /= (float)m_FrameIndex;

			accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y * m_Image->GetWidth()] = Utils::ConvertToABGR(accumulatedColor);
		});
	});
#else
	for (uint32_t y = 0; y < m_Image->GetHeight(); y++) {
		for (uint32_t x = 0; x < m_Image->GetWidth(); x++) {
			glm::vec4 color = PerPixel(x, y);
			m_AccumulationData[x + y * m_Image->GetWidth()] += color;

			glm::vec4 accumulatedColor = m_AccumulationData[x + y * m_Image->GetWidth()];
			accumulatedColor /= (float)m_FrameIndex;
			
			accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y * m_Image->GetWidth()] = Utils::ConvertToABGR(accumulatedColor);
		}
	}
#endif

	m_Image->SetData(m_ImageData);

	if (m_Settings.Accumulate)
		m_FrameIndex++;
	else
		m_FrameIndex = 1;
	
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

	delete[] m_AccumulationData;
	m_AccumulationData = new glm::vec4[width * height];
	
	m_ImageHorizontalIter.resize(width);
	m_ImageVerticalIter.resize(height);
	for (uint32_t i = 0; i < width; i++)
		m_ImageHorizontalIter[i] = i;
	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIter[i] = i;
}

Renderer::HitPayload Renderer::TraceRay(const Ray& ray) {
	int closestSphere = -1;
	float hitDistance = FLT_MAX;

	for (size_t i = 0; i < m_ActiveScene->Spheres.size(); i++) {
		const Sphere& sphere = m_ActiveScene->Spheres[i];
		float a = glm::dot(ray.Direction, ray.Direction);
		float b = 2.0f * glm::dot(ray.Origin, ray.Direction) - 2.0f * glm::dot(sphere.Position, ray.Direction); //-2.0f*glm::dot(circleOrigin,ray.Direction)
		float c = glm::dot(ray.Origin, ray.Origin) - sphere.Radius * sphere.Radius + glm::dot(sphere.Position, sphere.Position) - 2.0f * glm::dot(sphere.Position, ray.Origin);
		//+ glm::dot(circleOrigin, circleOrigin) - 2.0f * glm::dot(circleOrigin, ray.Origin)

		float d = b * b - 4.0f * a * c;

		if (d < 0.0f)
			continue;

		/*
		float t0 = (-b + glm::sqrt(d)) / (2.0f * a);
		float t1 = (-b - glm::sqrt(d)) / (2.0f * a);

		float tClosest;
		if (t0 < t1)
			tClosest = t0;
		else
			tClosest = t1;
		*/

		float tClosest = (-b - glm::sqrt(d)) / (2.0f * a);
		if (tClosest > 0.0f && tClosest < hitDistance) {
			hitDistance = tClosest;
			closestSphere = (int)i;
		}
	}

	if (closestSphere < 0)
		return Miss(ray);
	// (b_x^2 + b_y^2 + b_z^2)t^2 + (2a_xb_x - 2ab_x + 2a_y b_y - 2bb_y + 2a_zb_z - 2cb_z)t + (a_x^2 + a^2 - 2a·a_x + a_y^2 + b^2 - 2b·a_y + a_z^2 + c^2 - 2c·a_z - r^2)

	return ClosestHit(ray, hitDistance, closestSphere);
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y) {
	Ray ray;
	ray.Origin = m_ActiveCamera->GetPosition();
	ray.Direction = m_ActiveCamera->GetRayDirections()[x + y * m_Image->GetWidth()];

	glm::vec3 light(0.0f);
	glm::vec3 throughput(1.0f);

	int bounces = 7;
	for (int i = 0; i < bounces; i++) {
		Renderer::HitPayload payload = TraceRay(ray);

		if (payload.HitDistance < 0.0f) {
			glm::vec3 skyColor = glm::vec3(0.6f, 0.7f, 0.9f);
			light += skyColor * throughput;
			break;
		}


		const Sphere& sphere = m_ActiveScene->Spheres[payload.ObjectIndex];
		const Material& material = m_ActiveScene->Materials[sphere.matIndex];

		throughput *= material.Albedo;
		light += material.GetEmission() * material.Albedo;

		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;
		//ray.Direction = glm::reflect(ray.Direction, 
			//			payload.WorldNormal + material.Roughness * Walnut::Random::Vec3(-0.5f, 0.5f));
		ray.Direction = glm::normalize(payload.WorldNormal + Walnut::Random::InUnitSphere());
	}

	return glm::vec4(light, 1.0f);
}

Renderer::HitPayload Renderer::ClosestHit(const Ray& ray, float hitDistance, int objectIndex) {
	Renderer::HitPayload payload;
	payload.HitDistance = hitDistance;
	payload.ObjectIndex = objectIndex;

	const Sphere& closestSphere = m_ActiveScene->Spheres[objectIndex];

	payload.WorldPosition = ray.Origin + ray.Direction * hitDistance;
	payload.WorldNormal = glm::normalize(payload.WorldPosition - closestSphere.Position);

	return payload;
}

Renderer::HitPayload Renderer::Miss(const Ray& ray) {
	Renderer::HitPayload payload;
	payload.HitDistance = -1.0f;
	return payload;
}
