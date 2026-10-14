#pragma once
#include <array>
#include <concepts>
#include "PatchCode.h"
#include "PatchHeap.h"

/// @brief Class representing a collection of patch codes.
class PatchCodeCollection
{
private:
    using create_patch_code_func_t = PatchCode* (*)(PatchCodeCollection& patchCodeCollection);

public:
    PatchCodeCollection()
    {
        _patchCodeEntries.fill({ 0, nullptr });
    }

    PatchCodeCollection(const PatchCodeCollection&) = delete;

    /// @brief Adds a unique patch code to the collection. If the same patch code is added another time, it will be duplicated.
    /// @tparam T The type of patch code.
    /// @tparam ...Args The argument types for the patch code constructor.
    /// @param ...args The arguments for the patch code constructor.
    /// @return A pointer to the patch code.
    template <typename T, typename... Args> requires std::derived_from<T, PatchCode>
    const T* AddUniquePatchCode(Args&&... args)
    {
        T* patchCode = new T(std::forward<Args>(args)...);
        AddUniquePatchCode(patchCode);
        return patchCode;
    }

    /// @brief Gets or adds a shared patch code from/to the collection.
    ///        If the same patch code is requested another time, the same instance will be returned.
    /// @tparam T The type of patch code.
    /// @param factoryFunction A factory function that produces a pointer to the patch code class (T*).
    /// @return A pointer to the patch code.
    template <typename T> requires std::derived_from<std::remove_pointer_t<std::invoke_result_t<T>>, PatchCode>
    auto GetOrAddSharedPatchCode(T factoryFunction)
        // return type of the factory function converted from a pointer to a pointer to const
        -> std::add_pointer_t<std::add_const_t<std::remove_pointer_t<decltype(factoryFunction())>>>
    {
        u32 id = GetPatchCodeId<std::remove_pointer_t<decltype(factoryFunction())>>();
        auto& entry = GetOrAddSharedPatchCode(id);
        if (entry.type == 0)
        {
            entry.type = id; // claim this slot before calling the factory function
            entry.patchCode = factoryFunction();
        }

        return static_cast<decltype(factoryFunction())>(entry.patchCode);
    }

    /// @brief Checks whether a shared patch code of the given type is already in the collection.
    /// @tparam T The type of patch code.
    /// @return True if \see GetOrAddSharedPatchCode would return an existing instance.
    template <typename T> requires std::derived_from<T, PatchCode>
    bool HasSharedPatchCode() const
    {
        u32 id = GetPatchCodeId<T>();
        for (const auto& entry : _patchCodeEntries)
        {
            if (entry.type == id)
                return true;
        }
        return false;
    }

    /// @brief Copies all patch codes to their target location.
    void CopyAllToTarget() const;

private:
    struct PatchCodeEntry
    {
        u32 type;
        PatchCode* patchCode;
    };

    std::array<PatchCodeEntry, 64> _patchCodeEntries;

    void AddUniquePatchCode(PatchCode* patchCode);

    PatchCodeEntry& GetOrAddSharedPatchCode(u32 id);

    template <class T>
    static u32 GetPatchCodeId()
    {
        static const char typeId = 0;
        return (u32)&typeId;
    }
};