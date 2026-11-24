using System.Globalization;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Threading;

namespace KytyPS5;

internal sealed partial class ConsoleWindow
{
    private Button? _fpsRow;
    private DispatcherTimer? _fpsRepeat;
    private Key? _fpsHeldKey;
    private readonly HashSet<Key> _fpsKeys = [];
    private bool FpsRowFocused => _page == "settings" && _modal is null && _fpsRow is not null &&
        ReferenceEquals(FocusManager?.GetFocusedElement(), _fpsRow);

    private void StopFpsRepeat()
    {
        _fpsRepeat?.Stop();
        _fpsRepeat = null;
        _fpsHeldKey = null;
    }

    private void StartFpsRepeat(int direction)
    {
        StopFpsRepeat();
        if (!FpsRowFocused || !_adjust.TryGetValue(_fpsRow!, out var adjust)) return;
        bool CanStep()
        {
            var fps = FpsLimit.Read(_values["max_game_fps"]);
            return FpsRowFocused && FpsLimit.Step(fps, direction) != fps;
        }
        if (!CanStep()) return;
        adjust(direction);
        if (!CanStep()) return;
        var timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(FpsLimit.HoldDelay) };
        timer.Tick += (_, _) =>
        {
            if (!IsActive || !FpsRowFocused) { StopFpsRepeat(); return; }
            timer.Interval = TimeSpan.FromMilliseconds(FpsLimit.RepeatInterval);
            adjust(direction);
            if (!CanStep()) StopFpsRepeat();
        };
        _fpsRepeat = timer;
        timer.Start();
    }

    private void AddFpsLimit(StackPanel content)
    {
        const string id = "max_game_fps";
        var fps = FpsLimit.Read(_values[id]);
        var remembered = fps > 0 ? fps : FpsLimit.Read(_values.GetValueOrDefault("last_game_fps"));
        if (remembered == 0) remembered = FpsLimit.Default;
        var toggle = Setting("FPS limit", fps == 0 ? "Off" : "On");
        var row = Setting("Maximum game FPS", "", "20–120 FPS. Hold − or + to adjust continuously.");
        _fpsRow = row;
        row.LostFocus += (_, _) => StopFpsRepeat();
        var controls = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 14,
            HorizontalAlignment = HorizontalAlignment.Right };
        var grid = (Grid)row.Content!;
        grid.Children.Remove((TextBlock)row.Tag!);
        Grid.SetColumn(controls, 1);
        grid.Children.Add(controls);
        var minus = StepButton("−", -1);
        var number = Text("", 22);
        number.MinWidth = 48;
        number.TextAlignment = Avalonia.Media.TextAlignment.Center;
        var plus = StepButton("+", 1);
        controls.Children.Add(minus);
        controls.Children.Add(number);
        controls.Children.Add(plus);

        void Render()
        {
            UpdateRowValue(toggle, fps == 0 ? "Off" : "On");
            number.Text = remembered.ToString(CultureInfo.InvariantCulture);
            minus.IsEnabled = fps > FpsLimit.Minimum;
            plus.IsEnabled = fps > 0 && fps < FpsLimit.Maximum;
        }
        void Save(int next)
        {
            var last = next > 0 ? next : remembered;
            if (next == fps || !SaveValue(id, next.ToString(CultureInfo.InvariantCulture), last.ToString(CultureInfo.InvariantCulture))) return;
            remembered = last;
            fps = next;
            Render();
        }
        void Toggle() => Save(fps == 0 ? remembered : 0);
        toggle.Click += (_, _) => Toggle();
        _adjust[toggle] = direction => Save(direction < 0 ? 0 : remembered);
        _adjust[row] = direction => Save(FpsLimit.Step(fps, direction));
        content.Children.Add(toggle);
        content.Children.Add(row);
        Render();

        Button StepButton(string label, int direction)
        {
            var button = new Button { Content = label, Focusable = false, MinWidth = 56, FontSize = 24 };
            button.AddHandler(InputElement.PointerPressedEvent, (_, e) =>
            {
                if (!e.GetCurrentPoint(button).Properties.IsLeftButtonPressed) return;
                Focus(row);
                e.Pointer.Capture(button);
                StartFpsRepeat(direction);
                e.Handled = true;
            }, RoutingStrategies.Tunnel);
            button.AddHandler(InputElement.PointerReleasedEvent, (_, e) =>
            {
                if (e.Pointer.Captured != button) return;
                StopFpsRepeat();
                e.Pointer.Capture(null);
                e.Handled = true;
            }, RoutingStrategies.Tunnel);
            button.PointerExited += (_, e) =>
            {
                if (e.Pointer.Captured == button) e.Pointer.Capture(null);
            };
            button.PointerCaptureLost += (_, _) => StopFpsRepeat();
            return button;
        }
    }
}
