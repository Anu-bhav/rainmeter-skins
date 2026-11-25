using System.Text.Json;
using System.Text.Json.Serialization;
using RainmeterFreeze.Enumerations;

namespace RainmeterFreeze;

[JsonSourceGenerationOptions(
    WriteIndented = true,
    UseStringEnumConverter = true,
    AllowTrailingCommas = true,
    IgnoreReadOnlyProperties = true,
    PropertyNameCaseInsensitive = true,
    ReadCommentHandling = JsonCommentHandling.Skip
)]
[JsonSerializable(typeof(AppConfiguration))]
partial class JsonContext : JsonSerializerContext;

/// <summary>
/// Represents the configuration file of RainmeterFreeze.
/// </summary>
[Serializable]
public class AppConfiguration
{
    public static readonly string ConfigPath = Path.Combine(Program.DataFolderPath, "config.json");
    public static readonly string LogPath = Path.Combine(Program.DataFolderPath, "rainmeterfreeze.log");

    /// <summary>
    /// The freezing algorithm to use.
    /// </summary>
    public FreezeAlgorithm FreezeAlgorithm { get; set; } = FreezeAlgorithm.Maximized;

    /// <summary>
    /// The freeze mode to use.
    /// </summary>
    public FreezeMode FreezeMode { get; set; } = FreezeMode.Suspend;

    /// <summary>
    /// When 'true' (the default), Rainmeter is only frozen if the window that
    /// triggered the freeze is on the same monitor that Rainmeter is displayed
    /// on. This stops a maximized window on a secondary monitor from freezing
    /// Rainmeter while its skins are still visible on the primary monitor.
    /// </summary>
    public bool OnlyFreezeOnRainmeterMonitor { get; set; } = true;

    /// <summary>
    /// When 'true', writes debug events (freeze/unfreeze, foreground changes) to a log file.
    /// Log rotates at 1 MB. Default: false.
    /// </summary>
    public bool EnableDebugLog { get; set; } = false;

    /// <summary>
    /// Saves the configuration to the <see cref="ConfigPath"/>.
    /// </summary>
    public void Save()
    {
        string json = JsonSerializer.Serialize(this, JsonContext.Default.AppConfiguration);
        File.WriteAllText(ConfigPath, json);
    }

    /// <summary>
    /// Loads the application configuration from the <see cref="ConfigPath"/>.
    /// If no file exists under the expected location, a new instance of
    /// the <see cref="AppConfiguration"/> class is returned.
    /// </summary>
    /// <returns>The loaded <see cref="AppConfiguration"/>, or a new instance of it if no file has been found.</returns>
    public static AppConfiguration Load()
    {
        if (File.Exists(ConfigPath)) {
            string json = File.ReadAllText(ConfigPath);
            return JsonSerializer.Deserialize(json, JsonContext.Default.AppConfiguration) ?? new();
        }

        return new();
    }
}
