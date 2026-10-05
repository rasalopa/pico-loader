#include "common.h"
#include "ArmHelper.h"
#include "fastSearch.h"
#include "MainMemoryArenaHi.h"

// OS_GetInitArenaHi and OS_GetInitArenaLo both start with a switch on the arena id:
// cmp r0, #6; addls pc, pc, r0, lsl #2; b default; b case 0; ...; b case 6
static const u32 sArenaIdSwitchPattern[] = { 0xE3500006u, 0x908FF100u };

static constexpr s32 ARENA_ID_SWITCH_WORDS = 10;
static constexpr u32 ARM_ADD_SP_SP_4 = 0xE28DD004u; // some builds pop a word before the value
static constexpr u32 ARM_MOV_R0_ITCM_HI = 0xE3A00402u; // mov r0, #0x02000000
static constexpr u32 MAIN_MEMORY_ARENA_HI = 0x023E0000u; // the only value seen in this form

static constexpr s32 NOT_THE_SWITCH = -1;
static constexpr s32 UNSUPPORTED_FORM = -2;

/// @brief Returns the index of the first instruction of the case the b at \p branch
///        goes to, past an add sp, sp, #4, or -1 when that is not inside the image.
static s32 getCaseBody(const u32* image, s32 words, s32 branch)
{
    s32 body = branch + ArmHelper::GetArmCallOffset(image[branch]) / 4;
    if (body < 0 || body >= words)
        return -1;
    if (image[body] == ARM_ADD_SP_SP_4)
    {
        body++;
        if (body >= words)
            return -1;
    }
    return body;
}

/// @brief Checks whether the switch at \p arenaIdSwitch is the one of OS_GetInitArenaHi,
///        and whether it returns the main memory arena hi from a literal.
/// @return The index of that literal, NOT_THE_SWITCH to search on, or UNSUPPORTED_FORM
///         when it is OS_GetInitArenaHi but built some other way.
static s32 findMainMemoryArenaHiLiteral(const u32* image, s32 words, s32 arenaIdSwitch)
{
    if (arenaIdSwitch > words - ARENA_ID_SWITCH_WORDS)
        return NOT_THE_SWITCH;

    // the default and the seven cases are plain branches
    for (s32 i = 2; i < ARENA_ID_SWITCH_WORDS; i++)
    {
        if ((image[arenaIdSwitch + i] >> 24) != 0xEA)
            return NOT_THE_SWITCH;
    }

    // case 1, OS_ARENA_MAIN_SUBPRIV, only exists on the arm7, so it goes where the default goes
    if (ArmHelper::GetArmCallOffset(image[arenaIdSwitch + 4]) + 8
        != ArmHelper::GetArmCallOffset(image[arenaIdSwitch + 2]))
    {
        return NOT_THE_SWITCH;
    }

    // case 3, OS_ARENA_ITCM, returns the itcm hi here and the itcm lo in OS_GetInitArenaLo
    s32 itcmCase = getCaseBody(image, words, arenaIdSwitch + 6);
    if (itcmCase < 0 || image[itcmCase] != ARM_MOV_R0_ITCM_HI)
        return NOT_THE_SWITCH;

    // case 0, OS_ARENA_MAIN: ldr r0, [pc, #imm]
    s32 mainCase = getCaseBody(image, words, arenaIdSwitch + 3);
    if (mainCase < 0 || (image[mainCase] & 0xFFFFF000u) != 0xE59F0000u)
        return UNSUPPORTED_FORM;
    u32 literalOffset = image[mainCase] & 0xFFF;
    if (literalOffset & 3)
        return UNSUPPORTED_FORM;
    s32 literal = mainCase + 2 + (s32)(literalOffset / 4);
    if (literal >= words || image[literal] != MAIN_MEMORY_ARENA_HI)
        return UNSUPPORTED_FORM;
    return literal;
}

bool MainMemoryArenaHi::Find(u32* image, u32 imageSize)
{
    if (_searched)
        return _literal != nullptr;
    _searched = 1;

    s32 words = imageSize / 4;
    const u32* end = image + words;
    const u32* arenaIdSwitch = image;
    // OS_GetInitArenaLo starts the same way and usually comes first, so a switch
    // that is not the right one only means searching on from the next word.
    while ((arenaIdSwitch = fastSearch8(arenaIdSwitch, (end - arenaIdSwitch) * sizeof(u32), sArenaIdSwitchPattern)))
    {
        s32 literal = findMainMemoryArenaHiLiteral(image, words, arenaIdSwitch - image);
        if (literal >= 0)
        {
            _literal = &image[literal];
            _original = *_literal;
            break;
        }
        if (literal == UNSUPPORTED_FORM)
            break;
        arenaIdSwitch++;
    }

    if (_literal)
    {
        LOG_DEBUG("OS_GetInitArenaHi main memory literal at 0x%p\n", _literal);
    }
    else
    {
        LOG_DEBUG("OS_GetInitArenaHi main memory literal not found\n");
    }
    return _literal != nullptr;
}

void MainMemoryArenaHi::Lower(u32 arenaHi)
{
    if (!_literal)
        return;
    *_literal = arenaHi;
    _lowered = arenaHi;
}

void MainMemoryArenaHi::Restore()
{
    if (_lowered && *_literal == _lowered)
        *_literal = _original;
    _lowered = 0;
}
