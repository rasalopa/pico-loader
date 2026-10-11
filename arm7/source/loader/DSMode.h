#pragma once

/// @brief Class handling switching from DSi to DS mode.
class DSMode
{
public:
    /// @brief Switches to DS mode for the game with the given \p gameCode.
    /// @param gameCode The game code of the game to switch to DS mode for.
    /// @param keepDsiBios \c true to keep the dsi bios and leave scfg open, so
    ///        the exit combo can return to dsi mode, or \c false for the one way
    ///        switch to the ds bios with scfg locked.
    void SwitchToDSMode(u32 gameCode, bool keepDsiBios) const;

    /// @brief Checks whether main memory is still limited to the 4 MB of the DS,
    ///        which is the case after a soft reset from a game that kept the dsi bios.
    /// @return True when main memory is in ds mode.
    bool IsMainMemoryInDSMode() const;

    /// @brief Puts both cpus back in the configuration of a fresh dsi boot.
    void RestoreDSiMode() const;

    /// @brief Switches CODEC to DS mode for the game with the given \p gameCode.
    /// @param gameCode The game code of the game to switch CODEC to DS mode for.
    void SwitchToDSTouchAndSoundMode(u32 gameCode) const;

private:
    void SwitchCodecToDSMode(u32 gameCode) const;
    bool ShouldUseVolumeFix(u32 gameCode) const;
};
