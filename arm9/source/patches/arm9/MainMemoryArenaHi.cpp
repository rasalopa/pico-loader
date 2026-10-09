#include "common.h"
#include "ArmHelper.h"
#include "fastSearch.h"
#include "MainMemoryArenaHi.h"

// OS_GetInitArenaHi and OS_GetInitArenaLo both start with a switch on the arena id:
// cmp r0, #6; addls pc, pc, r0, lsl #2; b default; b case 0; ...; b case 6
static const u32 sArenaIdSwitchPattern[] = { 0xE3500006u, 0x908FF100u };

// Their thumb builds: cmp r0, #6; bhi default; adds r0, r0, r0; add r0, pc;
// ldrh r0, [r0, #k]; lsls r0, r0, #16; asrs r0, r0, #16; then add pc, r0, or
// add r0, pc and bx r0, before a table of halfword offsets to the cases. Two
// builds of it were seen, each always at the same alignment, so each is searched
// as two words it always has.
static const u32 sThumbArenaIdSwitchPatternA[] = { 0x88C04478u, 0x14000400u }; // from the add r0, pc
static const u32 sThumbArenaIdSwitchPatternB[] = { 0x44781800u, 0x04008900u }; // from the adds
static constexpr s32 THUMB_PATTERN_A_TO_SWITCH = -3; // halfwords from a match to its cmp
static constexpr s32 THUMB_PATTERN_B_TO_SWITCH = -2;

static constexpr s32 ARENA_ID_SWITCH_WORDS = 10;
static constexpr s32 THUMB_ARENA_ID_SWITCH_HALFWORDS = 9;
static constexpr u32 ARM_ADD_SP_SP_4 = 0xE28DD004u; // some builds pop a word before the value
static constexpr u32 ARM_MOV_R0_ITCM_HI = 0xE3A00402u; // mov r0, #0x02000000
static constexpr u32 ITCM_HI = 0x02000000u;
static constexpr u32 MAIN_MEMORY_ARENA_HI = 0x023E0000u; // the only value seen in these forms

// The sdk 2.0 builds make their value, 0x023C0000, as an immediate: mov r0, #0x023C0000;
// ldmfd sp!, {lr}; bx lr in the switch body, or mov r0, #0x023C0000; bx lr as a leaf
// case. The first is rewritten in place to subtract the distance and return with
// ldmfd sp!, {pc}, which interworks on the arm9. The second has no room, so the
// switch's b to it is pointed at a stub that does the same.
static constexpr u32 ARM_MOV_R0_SDK2_ARENA_HI = 0xE3A0078Fu;
static constexpr u32 SDK2_MAIN_MEMORY_ARENA_HI = 0x023C0000u;
static constexpr u32 ARM_LDMFD_SP_LR = 0xE8BD4000u;
static constexpr u32 ARM_LDMFD_SP_PC = 0xE8BD8000u;
static constexpr u32 ARM_BX_LR = 0xE12FFF1Eu;

static constexpr s32 NOT_THE_SWITCH = -1;
static constexpr s32 UNSUPPORTED_FORM = -2;

/// @brief Whether \p distance fits sub r0, r0, #distance as encoded below: any
///        multiple of 16 up to 0xFF0 does, as imm8 rotated right by 28.
static bool isSubR0Immediate(u32 distance)
{
    return distance != 0 && (distance & 0xF) == 0 && distance <= 0xFF0;
}

/// @brief sub r0, r0, #distance, for a distance \see isSubR0Immediate accepts.
static u32 makeSubR0(u32 distance)
{
    return 0xE2400E00u | (distance >> 4);
}

/// @brief An arm b at \p from to \p to.
static u32 makeArmBranch(const u32* from, const u32* to)
{
    s32 offset = (s32)((const u8*)to - (const u8*)from) - 8;
    return 0xEA000000u | (((u32)offset >> 2) & 0xFFFFFFu);
}

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
///        and how it returns the main memory arena hi.
/// @param form Receives the build it has, one of the FORM constants.
/// @return The index of the literal, of the mov r0 of the in place build, or of the
///         switch's b to the leaf case; NOT_THE_SWITCH to search on; UNSUPPORTED_FORM
///         when it is OS_GetInitArenaHi but built some other way.
static s32 findMainMemoryArenaHi(const u32* image, s32 words, s32 arenaIdSwitch, u32& form)
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

    // case 0, OS_ARENA_MAIN: ldr r0, [pc, #imm], or the sdk 2.0 immediate
    s32 mainCase = getCaseBody(image, words, arenaIdSwitch + 3);
    if (mainCase < 0)
        return UNSUPPORTED_FORM;
    if (image[mainCase] == ARM_MOV_R0_SDK2_ARENA_HI && mainCase + 2 < words)
    {
        if (image[mainCase + 1] == ARM_LDMFD_SP_LR && image[mainCase + 2] == ARM_BX_LR)
        {
            form = MainMemoryArenaHi::FORM_IMMEDIATE_IN_PLACE;
            return mainCase;
        }
        if (image[mainCase + 1] == ARM_BX_LR)
        {
            form = MainMemoryArenaHi::FORM_IMMEDIATE_LEAF;
            return arenaIdSwitch + 3;
        }
        return UNSUPPORTED_FORM;
    }
    if ((image[mainCase] & 0xFFFFF000u) != 0xE59F0000u)
        return UNSUPPORTED_FORM;
    u32 literalOffset = image[mainCase] & 0xFFF;
    if (literalOffset & 3)
        return UNSUPPORTED_FORM;
    s32 literal = mainCase + 2 + (s32)(literalOffset / 4);
    if (literal >= words || image[literal] != MAIN_MEMORY_ARENA_HI)
        return UNSUPPORTED_FORM;
    form = MainMemoryArenaHi::FORM_LITERAL;
    return literal;
}

/// @brief Returns the index of the literal the thumb ldr r0, [pc, #imm] at halfword
///        \p load reads, or -1 when it is not that instruction or the literal is not
///        inside the image.
static s32 getThumbLiteral(const u16* code, s32 words, s32 load)
{
    if ((code[load] & 0xFF00) != 0x4800)
        return -1;
    s32 literal = (load + 2) / 2 + (code[load] & 0xFF);
    return literal < words ? literal : -1;
}

/// @brief Returns the halfword index of the case the thumb switch at halfword
///        \p arenaIdSwitch goes to for \p arenaId, or -1 when that is not inside the image.
static s32 getThumbCaseBody(const u16* code, s32 halfwords, s32 arenaIdSwitch, s32 arenaId)
{
    s32 table = arenaIdSwitch + 5 + ((code[arenaIdSwitch + 4] >> 6) & 0x1F); // past the ldrh offset
    if (table + arenaId >= halfwords)
        return -1;
    // the offsets count from the pc that add pc, r0 or add r0, pc reads, 18 bytes past the cmp
    s32 body = arenaIdSwitch + 9 + ((s32)(s16)code[table + arenaId] >> 1);
    if (body < 0 || body > halfwords - 2)
        return -1;
    return body;
}

/// @brief The thumb twin of \see findMainMemoryArenaHiLiteral, for the switch whose
///        cmp is halfword \p arenaIdSwitch.
static s32 findThumbMainMemoryArenaHiLiteral(const u32* image, s32 words, s32 arenaIdSwitch)
{
    const u16* code = (const u16*)image;
    s32 halfwords = words * 2;
    if (arenaIdSwitch < 0 || arenaIdSwitch > halfwords - THUMB_ARENA_ID_SWITCH_HALFWORDS)
        return NOT_THE_SWITCH;

    const u16* switchCode = &code[arenaIdSwitch];
    if (switchCode[0] != 0x2806 || (switchCode[1] & 0xFF00) != 0xD800
        || switchCode[2] != 0x1800 || switchCode[3] != 0x4478 || (switchCode[4] & 0xF83F) != 0x8800
        || switchCode[5] != 0x0400 || switchCode[6] != 0x1400
        || (switchCode[7] != 0x4487 && (switchCode[7] != 0x4478 || switchCode[8] != 0x4700)))
    {
        return NOT_THE_SWITCH;
    }

    // case 1 goes where the default goes, as in the arm build
    s32 defaultCase = arenaIdSwitch + 3 + (s8)(switchCode[1] & 0xFF);
    s32 subPrivCase = getThumbCaseBody(code, halfwords, arenaIdSwitch, 1);
    if (subPrivCase < 0 || subPrivCase != defaultCase)
        return NOT_THE_SWITCH;

    // case 3 returns the itcm hi, from a literal or as movs r0, #2; lsls r0, r0, #24
    s32 itcmCase = getThumbCaseBody(code, halfwords, arenaIdSwitch, 3);
    if (itcmCase < 0)
        return NOT_THE_SWITCH;
    s32 itcmLiteral = getThumbLiteral(code, words, itcmCase);
    if ((itcmLiteral < 0 || image[itcmLiteral] != ITCM_HI)
        && (code[itcmCase] != 0x2002 || code[itcmCase + 1] != 0x0600))
    {
        return NOT_THE_SWITCH;
    }

    // case 0, OS_ARENA_MAIN: ldr r0, [pc, #imm]
    s32 mainCase = getThumbCaseBody(code, halfwords, arenaIdSwitch, 0);
    if (mainCase < 0)
        return UNSUPPORTED_FORM;
    s32 literal = getThumbLiteral(code, words, mainCase);
    if (literal < 0 || image[literal] != MAIN_MEMORY_ARENA_HI)
        return UNSUPPORTED_FORM;
    return literal;
}

/// @brief Hands every match of \p pattern to \p check until one is the switch of
///        OS_GetInitArenaHi. OS_GetInitArenaLo starts the same way, so a switch that
///        is not the right one only means searching on.
/// @return What \p check returned for that switch, or NOT_THE_SWITCH.
template<typename Check>
static s32 searchArenaIdSwitches(const u32* image, s32 words, const u32* pattern, Check check)
{
    const u32* end = image + words;
    const u32* match = image;
    while ((match = fastSearch8(match, (end - match) * sizeof(u32), pattern)))
    {
        s32 literal = check(match - image);
        if (literal != NOT_THE_SWITCH)
            return literal;
        match++;
    }
    return NOT_THE_SWITCH;
}

bool MainMemoryArenaHi::Find(u32* image, u32 imageSize)
{
    if (_searched)
        return _site != nullptr;
    _searched = 1;

    s32 words = imageSize / 4;
    u32 form = 0;
    s32 site = searchArenaIdSwitches(image, words, sArenaIdSwitchPattern, [=, &form] (s32 match)
    {
        return findMainMemoryArenaHi(image, words, match, form);
    });
    // A game with an arm OS_GetInitArenaHi has no thumb one, and most games have the
    // arm one, so only the others pay for searching the thumb builds.
    if (site == NOT_THE_SWITCH)
    {
        form = FORM_LITERAL;
        site = searchArenaIdSwitches(image, words, sThumbArenaIdSwitchPatternA, [=] (s32 match)
        {
            return findThumbMainMemoryArenaHiLiteral(image, words, match * 2 + THUMB_PATTERN_A_TO_SWITCH);
        });
    }
    if (site == NOT_THE_SWITCH)
    {
        site = searchArenaIdSwitches(image, words, sThumbArenaIdSwitchPatternB, [=] (s32 match)
        {
            return findThumbMainMemoryArenaHiLiteral(image, words, match * 2 + THUMB_PATTERN_B_TO_SWITCH);
        });
    }
    if (site >= 0)
    {
        _site = &image[site];
        _form = form;
        _original = form == FORM_LITERAL ? *_site : SDK2_MAIN_MEMORY_ARENA_HI;
    }

    if (_site)
    {
        LOG_DEBUG("OS_GetInitArenaHi main memory value from 0x%p, form %d\n", _site, _form);
    }
    else
    {
        LOG_DEBUG("OS_GetInitArenaHi main memory value not found\n");
    }
    return _site != nullptr;
}

bool MainMemoryArenaHi::CanLowerTo(u32 arenaHi) const
{
    if (!_site)
        return false;
    if (_form == FORM_LITERAL)
        return true;
    return isSubR0Immediate(_original - arenaHi);
}

void MainMemoryArenaHi::Lower(u32 arenaHi, u32* stub)
{
    if (!CanLowerTo(arenaHi))
        return;
    u32 distance = _original - arenaHi;
    switch (_form)
    {
        case FORM_LITERAL:
            *_site = arenaHi;
            break;
        case FORM_IMMEDIATE_IN_PLACE:
            // mov r0, #0x023C0000 stays; then sub r0, r0, #distance; ldmfd sp!, {pc}
            _saved[0] = _site[1];
            _saved[1] = _site[2];
            _written[0] = makeSubR0(distance);
            _written[1] = ARM_LDMFD_SP_PC;
            _site[1] = _written[0];
            _site[2] = _written[1];
            break;
        case FORM_IMMEDIATE_LEAF:
            // the stub is the leaf case with the subtraction in it
            stub[0] = ARM_MOV_R0_SDK2_ARENA_HI;
            stub[1] = makeSubR0(distance);
            stub[2] = ARM_BX_LR;
            _saved[0] = *_site;
            _written[0] = makeArmBranch(_site, stub);
            *_site = _written[0];
            break;
    }
    _lowered = arenaHi;
}

void MainMemoryArenaHi::Restore()
{
    if (_lowered)
    {
        switch (_form)
        {
            case FORM_LITERAL:
                if (*_site == _lowered)
                    *_site = _original;
                break;
            case FORM_IMMEDIATE_IN_PLACE:
                if (_site[1] == _written[0] && _site[2] == _written[1])
                {
                    _site[1] = _saved[0];
                    _site[2] = _saved[1];
                }
                break;
            case FORM_IMMEDIATE_LEAF:
                if (*_site == _written[0])
                    *_site = _saved[0];
                break;
        }
    }
    _lowered = 0;
}
