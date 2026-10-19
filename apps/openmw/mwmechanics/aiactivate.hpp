#ifndef GAME_MWMECHANICS_AIACTIVATE_H
#define GAME_MWMECHANICS_AIACTIVATE_H

#include "typedaipackage.hpp"
#include <components/esm/refid.hpp>

#include "../mwworld/ptr.hpp"

namespace ESM
{
    namespace AiSequence
    {
        struct AiActivate;
    }
}

namespace MWMechanics
{
    /// \brief Causes actor to walk to activatable object and activate it
    /** Will activate when close to object **/
    class AiActivate final : public TypedAiPackage<AiActivate>
    {
    public:
        /// Constructor
        /** \param objectId Reference to object to activate **/
        explicit AiActivate(const ESM::RefId& objectId, bool repeat);

        /// Construct an activation package for a specific, already resolved object.
        explicit AiActivate(const MWWorld::Ptr& object);

        explicit AiActivate(const ESM::AiSequence::AiActivate* activate);

        bool execute(const MWWorld::Ptr& actor, CharacterController& characterController, AiState& state,
            float duration) override;

        static constexpr AiPackageTypeId getTypeId() { return AiPackageTypeId::Activate; }

        void writeState(ESM::AiSequence::AiSequence& sequence) const override;

    private:
        ESM::RefId mObjectId;
        MWWorld::Ptr mObjectPtr;
    };
}
#endif // GAME_MWMECHANICS_AIACTIVATE_H
