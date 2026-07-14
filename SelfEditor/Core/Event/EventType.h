#pragma once

enum class EventType
{
    Enemy,
    Hold,
    Orb,
    Barrier,
    Rainbow,
};

enum class Wall
{
    Up,
    Left,
    Down,
    Right,
};

// Wall の宣言順 Up -> Left -> Down -> Right -> (Up) を CW（時計回り）の基準方向とする
enum class RotationDir
{
    CW,
    CCW,
};
