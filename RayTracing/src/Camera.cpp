#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Walnut/Input/Input.h"

using namespace Walnut;

Camera::Camera(float verticalFOV, float nearClip, float farClip) 
	: m_VerticalFOV(verticalFOV), m_NearClip(nearClip), m_FarClip(farClip)
{
	m_Direction = glm::vec3(0, 0, -1);
	m_Position = glm::vec3(6.5f, 6.5f, 25);
}

bool Camera::OnUpdate(float ts) {
	glm::vec2 mousePos = Input::GetMousePosition();
	glm::vec2 delta = (mousePos - m_LastMousePosition) * 0.002f;
	m_LastMousePosition = mousePos;

	if (!Input::IsMouseButtonDown(MouseButton::Right)) {
		Input::SetCursorMode(CursorMode::Normal);
		return false;
	}

	Input::SetCursorMode(CursorMode::Locked);

	bool moved = false;

	constexpr glm::vec3 upDirection(0.0f, 1.0f, 0.0f);
	glm::vec3 rightDirection = glm::cross(m_Direction, upDirection);

	float speed = 5.0f;

	if (Input::IsKeyDown(KeyCode::W)) {
		m_Position += ts * speed * m_Direction;
		moved = true;
	}
	if (Input::IsKeyDown(KeyCode::S)) {
		m_Position -= ts * speed * m_Direction;
		moved = true;
	}
	if (Input::IsKeyDown(KeyCode::A)) {
		m_Position -= ts * speed * rightDirection;
		moved = true;
	}
	if (Input::IsKeyDown(KeyCode::D)) {
		m_Position += ts * speed * rightDirection;
		moved = true;
	}
	if (Input::IsKeyDown(KeyCode::Space)) {
		m_Position += ts * speed * upDirection;
		moved = true;
	}
	if (Input::IsKeyDown(KeyCode::LeftControl)) {
		m_Position -= ts * speed * upDirection;
		moved = true;
	}

	if (delta.x != 0.0f || delta.y != 0.0f) {
		float pitchDelta = delta.y * GetRotationSpeed();
		float yawDelta = delta.x * GetRotationSpeed();

		glm::quat q = glm::normalize(glm::cross(glm::angleAxis(-pitchDelta, rightDirection), glm::angleAxis(-yawDelta, upDirection)));
		m_Direction = glm::rotate(q, m_Direction);

		moved = true;
	}

	if (moved) {
		RecalculateView();
		RecalculateRayDirection();
	}
	return moved;
}

void Camera::OnResize(uint32_t width, uint32_t height) {
	if (width == m_ViewportWidth && height == m_ViewportHeight)
		return;

	m_ViewportWidth = width;
	m_ViewportHeight = height;

	RecalculateProjection();
	RecalculateRayDirection();
}

float Camera::GetRotationSpeed() {
	return 0.3f;
}

void Camera::RecalculateProjection() {
	m_Projection = glm::perspectiveFov(glm::radians(m_VerticalFOV), (float)m_ViewportWidth, (float)m_ViewportHeight, m_NearClip, m_FarClip);
	m_InverseProjection = glm::inverse(m_Projection);
}

void Camera::RecalculateView() {
	m_View = glm::lookAt(m_Position, m_Position + m_Direction, glm::vec3(0, 1, 0));
	m_InverseView = glm::inverse(m_View);
}

void Camera::RecalculateRayDirection() {
	m_RayDirections.resize(m_ViewportWidth * m_ViewportHeight);

	for (uint32_t y = 0; y < m_ViewportHeight; y++) {
		for (uint32_t x = 0; x < m_ViewportWidth; x++) {
			glm::vec2 coord = { (float)x / (float)m_ViewportWidth, (float)y / (float)m_ViewportHeight };
			coord = coord * 2.0f - 1.0f; // -1 -> 1

			glm::vec4 target = m_InverseProjection * glm::vec4(coord.x, coord.y, 1, 1);
			glm::vec3 rayDirection = glm::vec3(m_InverseView * glm::vec4(glm::normalize(glm::vec3(target) / target.w), 0)); //World Space
			m_RayDirections[x + y * m_ViewportWidth] = rayDirection;
		}
	}
}