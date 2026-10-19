#ifndef GAME_MWMECHANICS_ACTIVESPELLS_H
#define GAME_MWMECHANICS_ACTIVESPELLS_H

#include <functional>
#include <list>
#include <queue>
#include <string>
#include <variant>
#include <vector>

#include <components/esm3/activespells.hpp>

#include "../mwworld/ptr.hpp"
#include "../mwworld/timestamp.hpp"

#include "spellcasting.hpp"

namespace ESM
{
    struct Enchantment;
    struct Spell;
}

namespace MWMechanics
{
    /// \brief Lasting spell effects
    ///
    /// \note The name of this class is slightly misleading, since it also handles lasting potion
    /// effects.
    class ActiveSpells
    {
    public:
        using ActiveEffect = ESM::ActiveEffect;
        class ActiveSpellParams
        {
            ESM::RefId mActiveSpellId;
            ESM::RefId mSourceSpellId;
            std::vector<ActiveEffect> mEffects;
            std::string mDisplayName;
            ESM::RefNum mCaster;
            ESM::RefNum mItem;
            ESM::ActiveSpells::Flags mFlags;
            int mWorsenings;
            MWWorld::TimeStamp mNextWorsening;
            MWWorld::Ptr mSource;

            ActiveSpellParams(const ESM::ActiveSpells::ActiveSpellParams& params);

            ActiveSpellParams(const ESM::Spell* spell, const MWWorld::Ptr& actor, bool ignoreResistances = false);

            ActiveSpellParams(
                const MWWorld::ConstPtr& item, const ESM::Enchantment* enchantment, const MWWorld::Ptr& actor);

            ActiveSpellParams(const ActiveSpellParams& params, const MWWorld::Ptr& actor);

            ESM::ActiveSpells::ActiveSpellParams toEsm() const;

            friend class ActiveSpells;

        public:
            ActiveSpellParams(
                const MWWorld::Ptr& caster, const ESM::RefId& id, std::string_view sourceName, ESM::RefNum item);

            ESM::RefId getActiveSpellId() const { return mActiveSpellId; }
            void setActiveSpellId(ESM::RefId id) { mActiveSpellId = id; }

            const ESM::RefId& getSourceSpellId() const { return mSourceSpellId; }

            const std::vector<ActiveEffect>& getEffects() const { return mEffects; }
            std::vector<ActiveEffect>& getEffects() { return mEffects; }

            ESM::RefNum getCaster() const { return mCaster; }

            int getWorsenings() const { return mWorsenings; }

            const std::string& getDisplayName() const { return mDisplayName; }

            ESM::RefNum getItem() const { return mItem; }
            ESM::RefId getEnchantment() const;

            const ESM::Spell* getSpell() const;
            bool hasFlag(ESM::ActiveSpells::Flags flags) const;
            void setFlag(ESM::ActiveSpells::Flags flags);

            // Increments worsenings count and sets the next timestamp
            void worsen();

            bool shouldWorsen() const;

            void resetWorsenings();
        };
            /*
                Start of tes3mp addition

                Track the actorId corresponding to these ActiveSpells
            */
            int mActorId = 0;
            /*
                End of tes3mp addition
            */

            void rebuildEffects() const;

        typedef std::list<ActiveSpellParams> Collection;
        typedef Collection::const_iterator TIterator;

        void readState(const ESM::ActiveSpells& state);
        void writeState(ESM::ActiveSpells& state) const;

        TIterator begin() const;

        TIterator end() const;

        TIterator getActiveSpellById(const ESM::RefId& id);

        void update(const MWWorld::Ptr& ptr, float duration);

    private:
        using ParamsPredicate = std::function<bool(const ActiveSpellParams&)>;
        using EffectPredicate = std::function<bool(const ActiveSpellParams&, const ESM::ActiveEffect&)>;
        using Predicate = std::variant<ParamsPredicate, EffectPredicate>;

        struct IterationGuard
        {
            ActiveSpells& mActiveSpells;

            IterationGuard(ActiveSpells& spells);
            ~IterationGuard();
        };
        struct UpdateContext;

        std::list<ActiveSpellParams> mSpells;
        std::vector<ActiveSpellParams> mQueue;
        std::queue<Predicate> mPurges;
        bool mIterating;

        void addToSpells(const MWWorld::Ptr& ptr, const ActiveSpellParams& spell, UpdateContext& context);

        bool applyPurges(const MWWorld::Ptr& ptr, std::list<ActiveSpellParams>::iterator* currentSpell = nullptr,
            std::vector<ActiveEffect>::iterator* currentEffect = nullptr);

        bool updateActiveSpell(
            const MWWorld::Ptr& ptr, float duration, Collection::iterator& spellIt, UpdateContext& context);

        bool initParams(const MWWorld::Ptr& ptr, const ActiveSpellParams& params, UpdateContext& context);

    public:
        ActiveSpells();

        /// Add lasting effects
        ///
        /// \brief addSpell
        /// \param id ID for stacking purposes.
        ///
        void addSpell(const ActiveSpellParams& params);

        /// Force resistances
        void addSpell(const ESM::Spell* spell, const MWWorld::Ptr& actor, bool ignoreResistances = true);
            /*
                Start of tes3mp addition

                Add a separate addSpell() with a timestamp argument
            */
            void addSpell (const std::string& id, bool stack, std::vector<ActiveEffect> effects,
                           const std::string& displayName, int casterActorId, MWWorld::TimeStamp timestamp, bool sendPacket = true);
            /*
                End of tes3mp addition
            */

            /// Removes the active effects from this spell/potion/.. with \a id
            void removeEffects (const std::string& id);

            /*
                Start of tes3mp addition

                Remove the spell with a certain ID and a certain timestamp, useful
                when there are stacked spells with the same ID
            */
            bool removeSpellByTimestamp(const std::string& id, MWWorld::TimeStamp timestamp);
            /*
                End of tes3mp addition
            */

            /// Remove all active effects with this effect id
            void purgeEffect (short effectId);

        /// Removes the active effects from this spell/potion/.. with \a id
        void removeEffectsBySourceSpellId(const MWWorld::Ptr& ptr, const ESM::RefId& id);
        /// Removes the active effects of a specific active spell
        void removeEffectsByActiveSpellId(const MWWorld::Ptr& ptr, const ESM::RefId& id);

        /// Remove all active effects with this effect id
        void purgeEffect(const MWWorld::Ptr& ptr, ESM::RefId effectId, ESM::RefId effectArg = {});

        void purge(EffectPredicate predicate, const MWWorld::Ptr& ptr);
        void purge(ParamsPredicate predicate, const MWWorld::Ptr& ptr);

        /// Remove all effects that were cast by \a actor
        void purge(const MWWorld::Ptr& ptr, ESM::RefNum actor);
            /*
                Start of tes3mp addition

                Allow the purging of an effect for a specific arg (attribute or skill)
            */
            void purgeEffectByArg(short effectId, int effectArg);
            /*
                End of tes3mp addition
            */

            /*
                Start of tes3mp addition

                Make it easy to get an effect's duration
            */
            float getEffectDuration(short effectId, std::string sourceId);
            /*
                End of tes3mp addition
            */

            /// Remove all spells
            void clear();

        /// Remove all spells
        void clear(const MWWorld::Ptr& ptr);

        /// True if a spell associated with this id is active
        /// \note For enchantments, this is the id of the enchanted item, not the enchantment itself
        bool isSpellActive(const ESM::RefId& id) const;

        /// True if the enchantment is active
        bool isEnchantmentActive(const ESM::RefId& id) const;

        void skipWorsenings(double hours);

        void unloadActor(const MWWorld::Ptr& ptr);
            /*
                Start of tes3mp addition

                Make it possible to set and get the actorId for these ActiveSpells
            */
            int getActorId() const;
            void setActorId(int actorId);
            /*
                End of tes3mp addition
            */

    };
}

#endif
