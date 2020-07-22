#include "Camera.h"
#include <GLFW\glfw3.h>

glm::mat4 Camera::GetViewMatrix()
{
    return glm::lookAt(Position, Position + Front, Up);
}

void Camera::ProcessKeyboard(Camera_Movement direction, float deltaTime)
{
    float velocity = MovementSpeed * deltaTime;
    //glm::vec3 bobbing = glm::vec3(0.0, sin(glfwGetTime() * 10.0f) * 0.025, 0.0);
    if (direction == FORWARD)
        Position += Front * velocity;// +bobbing;
    if (direction == BACKWARD)
        Position -= Front * velocity;// + bobbing;
    if (direction == LEFT)
        Position -= Right * velocity;// + bobbing;
    if (direction == RIGHT)
        Position += Right * velocity;// + bobbing;

    Position.y = 0.5f;
    
    /*if (Position.y < 0.0)
        Position.y = 0.0;
    if (Position.y > 1.0)
        Position.y = 1.0;*/
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch)
{
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw += xoffset;
    Pitch += yoffset;

    if (constrainPitch)
    {
        if (Pitch > 89.0f)
            Pitch = 89.0f;
        if (Pitch < -89.0f)
            Pitch = -89.0f;
    }

    UpdateCameraVectors();
}

void Camera::ProcessMouseScroll(float yoffset)
{
    /*if (Zoom >= 1.0f && Zoom <= 45.0f)
        Zoom -= yoffset;*/
    if (Zoom <= 1.0f)
        Zoom = 1.0f;
    if (Zoom >= 45.0f)
        Zoom = 45.0f;
}

void Camera::UpdateCameraVectors()
{
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));
}