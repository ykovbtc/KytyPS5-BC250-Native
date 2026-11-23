using System.Globalization;

namespace KytyPS5;

internal static class FpsLimit
{
    public const int Minimum = 20, Maximum = 120, Default = 60;
    public const int HoldDelay = 400, RepeatInterval = 200;

    public static int Read(string? stored) =>
        int.TryParse(stored?.Trim(), NumberStyles.None, CultureInfo.InvariantCulture, out var fps) && fps > 0
            ? Math.Clamp(fps, Minimum, Maximum) : 0;

    public static int Step(int fps, int direction) =>
        fps == 0 ? 0 : Math.Clamp(fps + direction, Minimum, Maximum);
}
