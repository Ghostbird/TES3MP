#include "spellcasting.hpp"

#include <components/esm3/loadench.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadstat.hpp>
#include <components/misc/constants.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/misc/rng.hpp>
#include <components/misc/strings/format.hpp>

/*
    Start of tes3mp addition

    Include additional headers for multiplayer purposes
*/
#include <components/openmw-mp/TimedLog.hpp>
#include "../mwmp/Main.hpp"
#include "../mwmp/Networking.hpp"
#include "../mwmp/PlayerList.hpp"
#include "../mwmp/LocalPlayer.hpp"
#include "../mwmp/ObjectList.hpp"
#include "../mwmp/CellController.hpp"
#include "../mwmp/MechanicsHelper.hpp"
/*
    End of tes3mp addition
*/

#include "../mwbase/windowmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/esmstore.hpp"

#include "../mwrender/animation.hpp"

#include "actorutil.hpp"
#include "creaturestats.hpp"
#include "spelleffects.hpp"
#include "spellutil.hpp"
#include "weapontype.hpp"

namespace MWMechanics
{
    CastSpell::CastSpell(
        const MWWorld::Ptr& caster, const MWWorld::Ptr& target, const bool fromProjectile, const bool scriptedSpell)
        : mCaster(caster)
        , mTarget(target)
        , mFromProjectile(fromProjectile)
        , mScriptedSpell(scriptedSpell)
    {
    }

    void CastSpell::explodeSpell(
        const ESM::EffectList& effects, const MWWorld::Ptr& ignore, ESM::RangeType rangeType) const
    {
        const auto world = MWBase::Environment::get().getWorld();
        std::map<MWWorld::Ptr, std::vector<ESM::IndexedENAMstruct>> toApply;
        for (const ESM::IndexedENAMstruct& effectInfo : effects.mList)
        {
            const ESM::MagicEffect* effect = world->getStore().get<ESM::MagicEffect>().find(effectInfo.mData.mEffectID);

            if (effectInfo.mData.mRange != rangeType
                || (effectInfo.mData.mArea <= 0 && !ignore.isEmpty() && ignore.getClass().isActor()))
                continue; // Not right range type, or not area effect and hit an actor

            if (mFromProjectile && effectInfo.mData.mArea <= 0)
                continue; // Don't play explosion for projectiles with 0-area effects

            if (!mFromProjectile && effectInfo.mData.mRange == ESM::RT_Touch && !ignore.isEmpty()
                && !ignore.getClass().isActor() && !ignore.getClass().hasToolTip(ignore)
                && (mCaster.isEmpty() || mCaster.getClass().isActor()))
                continue; // Don't play explosion for touch spells on non-activatable objects except when spell is from
                          // a projectile enchantment or ExplodeSpell

            // Spawn the explosion orb effect
            const ESM::Static* areaStatic;
            if (!effect->mArea.empty())
                areaStatic = world->getStore().get<ESM::Static>().find(effect->mArea);
            else
                areaStatic = world->getStore().get<ESM::Static>().find(ESM::RefId::stringRefId("VFX_DefaultArea"));

            const std::string& texture = effect->mParticle.getOriginal();

            if (effectInfo.mData.mArea <= 0)
            {
                if (effectInfo.mData.mRange == ESM::RT_Target)
                    world->spawnEffect(Misc::ResourceHelpers::correctMeshPath(areaStatic->mModel.getNormalized()),
                        texture, mHitPosition, 1.0f);
                continue;
            }
            else
                world->spawnEffect(Misc::ResourceHelpers::correctMeshPath(areaStatic->mModel.getNormalized()), texture,
                    mHitPosition, static_cast<float>(effectInfo.mData.mArea * 2));

            // Play explosion sound (make sure to use NoTrack, since we will delete the projectile now)
            {
                MWBase::SoundManager* sndMgr = MWBase::Environment::get().getSoundManager();
                if (!effect->mAreaSound.empty())
                    sndMgr->playSound3D(mHitPosition, effect->mAreaSound, 1.0f, 1.0f);
                else
                    sndMgr->playSound3D(mHitPosition,
                        world->getStore().get<ESM::Skill>().find(effect->mData.mSchool)->mSchool->mAreaSound, 1.0f,
                        1.0f);
            }
            // Get the actors in range of the effect
            std::vector<MWWorld::Ptr> objects;
            static const int unitsPerFoot = static_cast<int>(std::ceil(Constants::UnitsPerFoot));
            MWBase::Environment::get().getMechanicsManager()->getObjectsInRange(
                mHitPosition, static_cast<float>(effectInfo.mData.mArea * unitsPerFoot), objects);
            for (const MWWorld::Ptr& affected : objects)
            {
                // Ignore actors without collisions here, otherwise it will be possible to hit actors outside processing
                // range.
                if (affected.getClass().isActor() && !world->isActorCollisionEnabled(affected))
                    continue;

                auto& list = toApply[affected];
                list.push_back(effectInfo);
            }
        }

        // Now apply the appropriate effects to each actor in range
        for (auto& applyPair : toApply)
        {
            // Vanilla-compatible behaviour of never applying the spell to the caster
            // (could be changed by mods later)
            if (applyPair.first == mCaster)
                continue;

            if (applyPair.first == ignore)
                continue;

            ESM::EffectList effectsToApply;
            effectsToApply.mList = applyPair.second;
            inflict(applyPair.first, effectsToApply, rangeType, true);
        }
    }

    void CastSpell::launchMagicBolt() const
    {
        osg::Vec3f fallbackDirection(0, 1, 0);
        osg::Vec3f offset(0, 0, 0);
        if (!mTarget.isEmpty() && mTarget.getClass().isActor())
            offset.z() = MWBase::Environment::get().getWorld()->getHalfExtents(mTarget).z();

        // Fall back to a "caster to target" direction if we have no other means of determining it
        // (e.g. when cast by a non-actor)
        if (!mTarget.isEmpty())
            fallbackDirection = (mTarget.getRefData().getPosition().asVec3() + offset)
                - (mCaster.getRefData().getPosition().asVec3());

        MWBase::Environment::get().getWorld()->launchMagicBolt(mId, mCaster, fallbackDirection, mItem);
    }

    void CastSpell::inflict(
        const MWWorld::Ptr& target, const ESM::EffectList& effects, ESM::RangeType range, bool exploded) const
    {
        const bool targetIsActor = !target.isEmpty() && target.getClass().isActor();

        // If none of the effects need to apply, we can early-out
        bool found = false;
        bool containsRecastable = false;
        const auto& store = MWBase::Environment::get().getESMStore()->get<ESM::MagicEffect>();
        for (const ESM::IndexedENAMstruct& effect : effects.mList)
        {
            if (effect.mData.mRange == range)
            {
                found = true;
                const ESM::MagicEffect* magicEffect = store.find(effect.mData.mEffectID);
                if (!(magicEffect->mData.mFlags & ESM::MagicEffect::NonRecastable))
                    containsRecastable = true;
            }
        }
        if (!found)
            return;

        bool castByPlayer = (!mCaster.isEmpty() && mCaster == getPlayer());

        const ActiveSpells* targetSpells = nullptr;
        if (targetIsActor)
            targetSpells = &target.getClass().getCreatureStats(target).getActiveSpells();

        // Re-casting a bound equipment effect has no effect if the spell is still active
        if (!containsRecastable && targetSpells && targetSpells->isSpellActive(mId))
        {
            if (castByPlayer)
                MWBase::Environment::get().getWindowManager()->messageBox("#{sMagicCannotRecast}");
            return;
        }

        std::vector<int> indexes;

        for (auto& enam : effects.mList)
        {
            if (target.isEmpty())
                break;

            if (enam.mData.mRange != range)
                continue;
            const ESM::MagicEffect* magicEffect = store.find(enam.mData.mEffectID);
            if (!magicEffect)
                continue;
            // caster needs to be an actor for linked effects (e.g. Absorb)
            if (magicEffect->mData.mFlags & ESM::MagicEffect::CasterLinked
                && (mCaster.isEmpty() || !mCaster.getClass().isActor()))
                continue;

            indexes.push_back(enam.mIndex);
                    && (caster.isEmpty() || !caster.getClass().isActor()))
                continue;

            // Notify the target actor they've been hit
            bool isHarmful = magicEffect->mData.mFlags & ESM::MagicEffect::Harmful;
            if (target.getClass().isActor() && target != caster && !caster.isEmpty() && isHarmful)
                target.getClass().onHit(target, 0.0f, true, MWWorld::Ptr(), caster, osg::Vec3f(), true);

            // Reflect harmful effects
            if (!reflected && reflectEffect(*effectIt, magicEffect, caster, target, reflectedEffects))
                continue;

            /*
                Start of tes3mp addition

                Now that reflected effects have been handled, don't unilaterally process effects further for dedicated players
                and actors on this client and instead expect their effects to be applied correctly through the SpellsActive
                packets received
            */
            if (mwmp::PlayerList::isDedicatedPlayer(target) || mwmp::Main::get().getCellController()->isDedicatedActor(target))
                continue;
            /*
                End of tes3mp addition
            */

            // Try resisting.
            float magnitudeMult = getEffectMultiplier(effectIt->mEffectID, target, caster, spell, &targetEffects);
            if (magnitudeMult == 0)
            {
                // Fully resisted, show message
                if (target == getPlayer())
                    MWBase::Environment::get().getWindowManager()->messageBox("#{sMagicPCResisted}");
                else if (castByPlayer)
                    MWBase::Environment::get().getWindowManager()->messageBox("#{sMagicTargetResisted}");
            }
            else
            {
                float magnitude = effectIt->mMagnMin + Misc::Rng::rollDice(effectIt->mMagnMax - effectIt->mMagnMin + 1);
                magnitude *= magnitudeMult;

                if (!target.getClass().isActor())
                {
                    // non-actor objects have no list of active magic effects, so have to apply instantly
                    if (!applyInstantEffect(target, caster, EffectKey(*effectIt), magnitude))
                        continue;
                }
                else // target.getClass().isActor() == true
                {
                    ActiveSpells::ActiveEffect effect;
                    effect.mEffectId = effectIt->mEffectID;
                    effect.mArg = MWMechanics::EffectKey(*effectIt).mArg;
                    effect.mMagnitude = magnitude;
                    effect.mTimeLeft = 0.f;
                    effect.mEffectIndex = currentEffectIndex;

                    // Avoid applying absorb effects if the caster is the target
                    // We still need the spell to be added
                    if (caster == target
                        && effectIt->mEffectID >= ESM::MagicEffect::AbsorbAttribute
                        && effectIt->mEffectID <= ESM::MagicEffect::AbsorbSkill)
                    {
                        effect.mMagnitude = 0;
                    }

                    // Avoid applying harmful effects to the player in god mode
                    if (target == getPlayer() && MWBase::Environment::get().getWorld()->getGodModeState() && isHarmful)
                    {
                        effect.mMagnitude = 0;
                    }

                    bool effectAffectsHealth = isHarmful || effectIt->mEffectID == ESM::MagicEffect::RestoreHealth;
                    if (castByPlayer && target != caster && !target.getClass().getCreatureStats(target).isDead() && effectAffectsHealth)
                    {
                        // If player is attempting to cast a harmful spell on or is healing a living target, show the target's HP bar.
                        MWBase::Environment::get().getWindowManager()->setEnemy(target);
                    }

                    bool hasDuration = !(magicEffect->mData.mFlags & ESM::MagicEffect::NoDuration);
                    effect.mDuration = hasDuration ? static_cast<float>(effectIt->mDuration) : 1.f;

                    bool appliedOnce = magicEffect->mData.mFlags & ESM::MagicEffect::AppliedOnce;
                    if (!appliedOnce)
                        effect.mDuration = std::max(1.f, effect.mDuration);

                    if (effect.mDuration == 0)
                    {
                        // We still should add effect to list to allow GetSpellEffects to detect this spell
                        appliedLastingEffects.push_back(effect);

                        // duration 0 means apply full magnitude instantly
                        bool wasDead = target.getClass().getCreatureStats(target).isDead();
                        effectTick(target.getClass().getCreatureStats(target), target, EffectKey(*effectIt), effect.mMagnitude);
                        bool isDead = target.getClass().getCreatureStats(target).isDead();

                        /*
                            Start of tes3mp addition

                            If the target was a LocalPlayer or LocalActor who died, record the caster as the killer
                        */
                        if (!wasDead && isDead)
                        {
                            bool isSuicide = target == caster || caster.isEmpty();

                            if (target == MWMechanics::getPlayer())
                            {
                                mwmp::Main::get().getLocalPlayer()->killer = isSuicide ?
                                    MechanicsHelper::getTarget(target) : MechanicsHelper::getTarget(caster);
                            }
                            else if (mwmp::Main::get().getCellController()->isLocalActor(target))
                            {
                                mwmp::Main::get().getCellController()->getLocalActor(target)->killer = isSuicide ?
                                    MechanicsHelper::getTarget(target) : MechanicsHelper::getTarget(caster);
                            }
                        }
                        /*
                            End of tes3mp addition
                        */

                        if (!wasDead && isDead)
                            MWBase::Environment::get().getMechanicsManager()->actorKilled(target, caster);
                    }
                    else
                    {
                        effect.mTimeLeft = effect.mDuration;

                        targetEffects.add(MWMechanics::EffectKey(*effectIt), MWMechanics::EffectParam(effect.mMagnitude));

                        // add to list of active effects, to apply in next frame
                        appliedLastingEffects.push_back(effect);

                        // Unequip all items, if a spell with the ExtraSpell effect was casted
                        if (effectIt->mEffectID == ESM::MagicEffect::ExtraSpell && target.getClass().hasInventoryStore(target))
                        {
                            MWWorld::InventoryStore& store = target.getClass().getInventoryStore(target);
                            store.unequipAll(target);
                        }

                        // Command spells should have their effect, including taking the target out of combat, each time the spell successfully affects the target
                        if (((effectIt->mEffectID == ESM::MagicEffect::CommandHumanoid && target.getClass().isNpc())
                        || (effectIt->mEffectID == ESM::MagicEffect::CommandCreature && target.getTypeName() == typeid(ESM::Creature).name()))
                        && !caster.isEmpty() && caster.getClass().isActor() && target != getPlayer() && effect.mMagnitude >= target.getClass().getCreatureStats(target).getLevel())
                        {
                            MWMechanics::AiFollow package(caster, true);
                            target.getClass().getCreatureStats(target).getAiSequence().stack(package, target);
                        }

                        // For absorb effects, also apply the effect to the caster - but with a negative
                        // magnitude, since we're transferring stats from the target to the caster
                        if (effectIt->mEffectID >= ESM::MagicEffect::AbsorbAttribute && effectIt->mEffectID <= ESM::MagicEffect::AbsorbSkill)
                            absorbStat(*effectIt, effect, caster, target, reflected, mSourceName);
                    }
                }

                // Re-casting a summon effect will remove the creature from previous castings of that effect.
                if (isSummoningEffect(effectIt->mEffectID) && targetIsActor)
                {
                    CreatureStats& targetStats = target.getClass().getCreatureStats(target);
                    ESM::SummonKey key(effectIt->mEffectID, mId, currentEffectIndex);
                    auto findCreature = targetStats.getSummonedCreatureMap().find(key);
                    if (findCreature != targetStats.getSummonedCreatureMap().end())
                    {
                        /*
                            Start of tes3mp change (major)

                            Don't clean up placeholder summoned creatures still awaiting a spawn
                            packet from the server, because that would make the packet create permanent
                            spawns instead
                        */
                        if (findCreature->second != -1)
                        {
                            MWBase::Environment::get().getMechanicsManager()->cleanupSummonedCreature(target, findCreature->second);
                            targetStats.getSummonedCreatureMap().erase(findCreature);
                        }
                        /*
                            End of tes3mp change (major)
                        */
                    }
                }

                if (target.getClass().isActor() || magicEffect->mData.mFlags & ESM::MagicEffect::NoDuration)
                {
                    static const std::string schools[] = {
                        "alteration", "conjuration", "destruction", "illusion", "mysticism", "restoration"
                    };

                    MWBase::SoundManager *sndMgr = MWBase::Environment::get().getSoundManager();
                    if(!magicEffect->mHitSound.empty())
                        sndMgr->playSound3D(target, magicEffect->mHitSound, 1.0f, 1.0f);
                    else
                        sndMgr->playSound3D(target, schools[magicEffect->mData.mSchool]+" hit", 1.0f, 1.0f);

                    /*
                        Start of tes3mp addition

                        Send an ID_OBJECT_SOUND packet every time a sound is made here
                    */
                    mwmp::ObjectList* objectList = mwmp::Main::get().getNetworking()->getObjectList();
                    objectList->reset();
                    objectList->packetOrigin = mwmp::CLIENT_GAMEPLAY;
                    objectList->addObjectSound(target, magicEffect->mHitSound.empty() ? schools[magicEffect->mData.mSchool] + " hit" : magicEffect->mHitSound, 1.0f, 1.0f);
                    objectList->sendObjectSound();
                    /*
                        End of tes3mp addition
                    */

                    // Add VFX
                    const ESM::Static* castStatic;
                    if (!magicEffect->mHit.empty())
                        castStatic = MWBase::Environment::get().getWorld()->getStore().get<ESM::Static>().find (magicEffect->mHit);
                    else
                        castStatic = MWBase::Environment::get().getWorld()->getStore().get<ESM::Static>().find ("VFX_DefaultHit");

                    bool loop = (magicEffect->mData.mFlags & ESM::MagicEffect::ContinuousVfx) != 0;
                    // Note: in case of non actor, a free effect should be fine as well
                    MWRender::Animation* anim = MWBase::Environment::get().getWorld()->getAnimation(target);
                    if (anim && !castStatic->mModel.empty())
                        anim->addEffect("meshes\\" + castStatic->mModel, magicEffect->mIndex, loop, "", magicEffect->mParticle);
                }
            }
        }

        MWBase::Environment::get().getLuaManager()->applyMagicEffects(mId, mCaster, mItem, target, indexes,
            mScriptedSpell, false, mFlags & ESM::ActiveSpells::Flag_Stackable, false);

        if (!exploded)
            explodeSpell(effects, target, range);
        if (!target.isEmpty())
        {
            if (!reflectedEffects.mList.empty())
                inflict(caster, target, reflectedEffects, range, true, exploded);

            if (!appliedLastingEffects.empty())
            {
                int casterActorId = -1;
                if (!caster.isEmpty() && caster.getClass().isActor())
                    casterActorId = caster.getClass().getCreatureStats(caster).getActorId();
                target.getClass().getCreatureStats(target).getActiveSpells().addSpell(mId, mStack, appliedLastingEffects,
                        mSourceName, casterActorId);
            }
        }
    }

    bool CastSpell::applyInstantEffect(const MWWorld::Ptr &target, const MWWorld::Ptr &caster, const MWMechanics::EffectKey& effect, float magnitude)
    {
        short effectId = effect.mId;
        if (target.getClass().canLock(target))
        {
            if (effectId == ESM::MagicEffect::Lock)
            {
                const MWWorld::ESMStore& store = MWBase::Environment::get().getWorld()->getStore();
                const ESM::MagicEffect *magiceffect = store.get<ESM::MagicEffect>().find(effectId);
                MWRender::Animation* animation = MWBase::Environment::get().getWorld()->getAnimation(target);
                if (animation)
                    animation->addSpellCastGlow(magiceffect);
                if (target.getCellRef().getLockLevel() < magnitude) //If the door is not already locked to a higher value, lock it to spell magnitude
                {
                    if (caster == getPlayer())
                        MWBase::Environment::get().getWindowManager()->messageBox("#{sMagicLockSuccess}");

                    /*
                        Start of tes3mp change (major)

                        Disable unilateral locking on this client and expect the server's reply to our
                        packet to do it instead
                    */
                    //target.getCellRef().lock(static_cast<int>(magnitude));
                    /*
                        End of tes3mp change (major)
                    */

                    /*
                        Start of tes3mp addition

                        Send an ID_OBJECT_LOCK packet every time an object is locked here
                    */
                    mwmp::ObjectList *objectList = mwmp::Main::get().getNetworking()->getObjectList();
                    objectList->reset();
                    objectList->packetOrigin = mwmp::CLIENT_GAMEPLAY;
                    objectList->addObjectLock(target, static_cast<int>(magnitude));
                    objectList->sendObjectLock();
                    /*
                        End of tes3mp addition
                    */
                }
                return true;
            }
            else if (effectId == ESM::MagicEffect::Open)
            {
                if (!caster.isEmpty())
                {
                    MWBase::Environment::get().getMechanicsManager()->unlockAttempted(getPlayer(), target);
                    // Use the player instead of the caster for vanilla crime compatibility
                }
                const MWWorld::ESMStore& store = MWBase::Environment::get().getWorld()->getStore();
                const ESM::MagicEffect *magiceffect = store.get<ESM::MagicEffect>().find(effectId);
                MWRender::Animation* animation = MWBase::Environment::get().getWorld()->getAnimation(target);
                if (animation)
                    animation->addSpellCastGlow(magiceffect);
                if (target.getCellRef().getLockLevel() <= magnitude)
                {
                    if (target.getCellRef().getLockLevel() > 0)
                    {
                        MWBase::Environment::get().getSoundManager()->playSound3D(target, "Open Lock", 1.f, 1.f);

                        if (caster == getPlayer())
                            MWBase::Environment::get().getWindowManager()->messageBox("#{sMagicOpenSuccess}");
                    }

                    /*
                        Start of tes3mp change (major)

                        Disable unilateral locking on this client and expect the server's reply to our
                        packet to do it instead
                    */
                    //target.getCellRef().unlock();
                    /*
                        End of tes3mp change (major)
                    */

                    /*
                        Start of tes3mp addition

                        Send an ID_OBJECT_LOCK packet every time an object is unlocked here
                    */
                    mwmp::ObjectList *objectList = mwmp::Main::get().getNetworking()->getObjectList();
                    objectList->reset();
                    objectList->packetOrigin = mwmp::CLIENT_GAMEPLAY;
                    objectList->addObjectLock(target, 0);
                    objectList->sendObjectLock();
                    /*
                        End of tes3mp addition
                    */
                }
                else
                {
                    MWBase::Environment::get().getSoundManager()->playSound3D(target, "Open Lock Fail", 1.f, 1.f);
                }

                return true;
            }
        }
        else if (target.getClass().isActor() && effectId == ESM::MagicEffect::Dispel)
        {
            target.getClass().getCreatureStats(target).getActiveSpells().purgeAll(magnitude, true);
            return true;
        }
        else if (target.getClass().isActor() && target == getPlayer())
        {
            MWRender::Animation* anim = MWBase::Environment::get().getWorld()->getAnimation(mCaster);
            bool teleportingEnabled = MWBase::Environment::get().getWorld()->isTeleportingEnabled();

            if (effectId == ESM::MagicEffect::DivineIntervention || effectId == ESM::MagicEffect::AlmsiviIntervention)
            {
                if (!teleportingEnabled)
                {
                    if (caster == getPlayer())
                        MWBase::Environment::get().getWindowManager()->messageBox("#{sTeleportDisabled}");
                    return true;
                }
                std::string marker = (effectId == ESM::MagicEffect::DivineIntervention) ? "divinemarker" : "templemarker";
                MWBase::Environment::get().getWorld()->teleportToClosestMarker(target, marker);
                anim->removeEffect(effectId);
                const ESM::Static* fx = MWBase::Environment::get().getWorld()->getStore().get<ESM::Static>()
                    .search("VFX_Summon_end");
                if (fx)
                    anim->addEffect("meshes\\" + fx->mModel, -1);
                return true;
            }
            else if (effectId == ESM::MagicEffect::Mark)
            {
                if (teleportingEnabled)
                {
                    MWBase::Environment::get().getWorld()->getPlayer().markPosition(
                        target.getCell(), target.getRefData().getPosition());
                }
                else if (caster == getPlayer())
                {
                    MWBase::Environment::get().getWindowManager()->messageBox("#{sTeleportDisabled}");
                }

                /*
                    Start of tes3mp addition

                    Send a PlayerMiscellaneous packet with the player's new mark location
                */
                mwmp::Main::get().getLocalPlayer()->sendMarkLocation(*target.getCell()->getCell(), target.getRefData().getPosition());
                /*
                    End of tes3mp addition
                */

                return true;
            }
            else if (effectId == ESM::MagicEffect::Recall)
            {
                if (!teleportingEnabled)
                {
                    if (caster == getPlayer())
                        MWBase::Environment::get().getWindowManager()->messageBox("#{sTeleportDisabled}");
                    return true;
                }

                MWWorld::CellStore* markedCell = nullptr;
                ESM::Position markedPosition;

                MWBase::Environment::get().getWorld()->getPlayer().getMarkedPosition(markedCell, markedPosition);
                if (markedCell)
                {
                    MWWorld::ActionTeleport action(markedCell->isExterior() ? "" : markedCell->getCell()->mName,
                                            markedPosition, false);
                    action.execute(target);
                    anim->removeEffect(effectId);
                }
                return true;
            }
        }
        return false;
    }

    bool CastSpell::cast(const ESM::RefId& id)
    {
        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
        if (const auto spell = store.get<ESM::Spell>().search(id))
            return cast(spell);

        if (const auto potion = store.get<ESM::Potion>().search(id))
            return cast(potion);

        if (const auto ingredient = store.get<ESM::Ingredient>().search(id))
            return cast(ingredient);

        throw std::runtime_error("ID type cannot be casted");
    }

    bool CastSpell::cast(const MWWorld::Ptr& item, bool launchProjectile)
    {
        const ESM::RefId& enchantmentName = item.getClass().getEnchantment(item);
        if (enchantmentName.empty())
            throw std::runtime_error("can't cast an item without an enchantment");

        mSourceName = item.getClass().getName(item);
        mId = item.getCellRef().getRefId();

        const auto& store = MWBase::Environment::get().getESMStore();
        const ESM::Enchantment* enchantment = store->get<ESM::Enchantment>().find(enchantmentName);

        // CastOnce enchantments (i.e. scrolls) never stack and the item is immediately destroyed,
        // so don't track the source item.
        if (enchantment->mData.mType != ESM::Enchantment::CastOnce)
            mItem = item.getCellRef().getRefNum();

        bool godmode = mCaster == MWMechanics::getPlayer() && MWBase::Environment::get().getWorld()->getGodModeState();
        bool isProjectile = false;
        if (item.getType() == ESM::Weapon::sRecordId)
        {
            const ESM::RefId type = item.get<ESM::Weapon>()->mBase->mData.mType;
            ESM::WeaponType::Class weapclass = MWMechanics::getWeaponType(type)->mWeaponClass;
            isProjectile = (weapclass == ESM::WeaponType::Thrown || weapclass == ESM::WeaponType::Ammo);
        }
        int type = enchantment->mData.mType;

        // Check if there's enough charge left
        if (!godmode
            && (type == ESM::Enchantment::WhenUsed || (!isProjectile && type == ESM::Enchantment::WhenStrikes)))
        {
            int castCost = getEffectiveEnchantmentCastCost(*enchantment, mCaster);

            if (item.getCellRef().getEnchantmentCharge() == -1)
                item.getCellRef().setEnchantmentCharge(
                    static_cast<float>(MWMechanics::getEnchantmentCharge(*enchantment)));

            if (item.getCellRef().getEnchantmentCharge() < castCost)
            {
                if (mCaster == getPlayer())
                {
                    MWBase::Environment::get().getWindowManager()->messageBox("#{sMagicInsufficientCharge}");

                    // Failure sound
                    ESM::RefId school = ESM::Skill::Alteration;
                    if (!enchantment->mEffects.mList.empty())
                    {
                        ESM::RefId effectId = enchantment->mEffects.mList.front().mData.mEffectID;
                        const ESM::MagicEffect* magicEffect = store->get<ESM::MagicEffect>().find(effectId);
                        school = magicEffect->mData.mSchool;
                    }

                    MWBase::SoundManager* sndMgr = MWBase::Environment::get().getSoundManager();
                    sndMgr->playSound3D(
                        mCaster, store->get<ESM::Skill>().find(school)->mSchool->mFailureSound, 1.0f, 1.0f);
                    static const std::string schools[] = {
                        "alteration", "conjuration", "destruction", "illusion", "mysticism", "restoration"
                    };
                    MWBase::SoundManager *sndMgr = MWBase::Environment::get().getSoundManager();
                    sndMgr->playSound3D(mCaster, "Spell Failure " + schools[school], 1.0f, 1.0f);

                    /*
                        Start of tes3mp addition

                        Send an ID_OBJECT_SOUND packet every time a sound is made here
                    */
                    mwmp::ObjectList* objectList = mwmp::Main::get().getNetworking()->getObjectList();
                    objectList->reset();
                    objectList->packetOrigin = mwmp::CLIENT_GAMEPLAY;
                    objectList->addObjectSound(mCaster, "Spell Failure " + schools[school], 1.0f, 1.0f);
                    objectList->sendObjectSound();
                    /*
                        End of tes3mp addition
                    */
                }
                return false;
            }
            // Reduce charge
            item.getCellRef().setEnchantmentCharge(item.getCellRef().getEnchantmentCharge() - castCost);
        }

        if (type == ESM::Enchantment::WhenUsed)
        {
            if (mCaster == getPlayer())
                mCaster.getClass().skillUsageSucceeded(mCaster, ESM::Skill::Enchant, ESM::Skill::Enchant_UseMagicItem);
        }
        else if (type == ESM::Enchantment::CastOnce)
        {
            if (!godmode)
                item.getContainerStore()->remove(item, 1);
        }
        else if (type == ESM::Enchantment::WhenStrikes)
        {
            if (mCaster == getPlayer())
                mCaster.getClass().skillUsageSucceeded(mCaster, ESM::Skill::Enchant, ESM::Skill::Enchant_CastOnStrike);
        }

        if (isProjectile)
            inflict(mTarget, enchantment->mEffects, ESM::RT_Self);
        else
            inflict(mCaster, enchantment->mEffects, ESM::RT_Self);

        if (isProjectile || !mTarget.isEmpty())
            inflict(mTarget, enchantment->mEffects, ESM::RT_Touch);

        if (launchProjectile)
            launchMagicBolt();
        else if (isProjectile || !mTarget.isEmpty())
            inflict(mTarget, enchantment->mEffects, ESM::RT_Target);

        return true;
    }

    bool CastSpell::cast(const ESM::Potion* potion)
    {
        mSourceName = potion->mName;
        mId = potion->mId;
        mFlags = static_cast<ESM::ActiveSpells::Flags>(
            ESM::ActiveSpells::Flag_Temporary | ESM::ActiveSpells::Flag_Stackable);

        // Ignore range and don't apply area of effect
        inflict(mCaster, potion->mEffects, ESM::RT_Self, true);
        inflict(mCaster, potion->mEffects, ESM::RT_Touch, true);
        inflict(mCaster, potion->mEffects, ESM::RT_Target, true);

        return true;
    }

    bool CastSpell::cast(const ESM::Spell* spell)
    {
        mSourceName = spell->mName;
        mId = spell->mId;

        ESM::RefId school = ESM::Skill::Alteration;

        bool godmode = mCaster == MWMechanics::getPlayer() && MWBase::Environment::get().getWorld()->getGodModeState();

        if (mCaster.getClass().isActor() && !mAlwaysSucceed && !mScriptedSpell)
        {
            school = getSpellSchool(spell, mCaster);

            CreatureStats& stats = mCaster.getClass().getCreatureStats(mCaster);

            if (!godmode)
            {
                bool fail = false;

                /*
                    Start of tes3mp change (major)
                
                    Make spell casting fail based on the casting success rated determined
                    in MechanicsHelper::getSpellSuccess()
                */
                mwmp::Cast *localCast = NULL;
                mwmp::Cast *dedicatedCast = MechanicsHelper::getDedicatedCast(mCaster);

                if (dedicatedCast)
                    dedicatedCast->pressed = false;
                else
                {
                    localCast = MechanicsHelper::getLocalCast(mCaster);
                    localCast->success = MechanicsHelper::getSpellSuccess(mId, mCaster);
                    localCast->pressed = false;
                    localCast->shouldSend = true;
                }

                // Check success
                float successChance = getSpellSuccessChance(spell, mCaster, nullptr, true, false);
                auto& prng = MWBase::Environment::get().getWorld()->getPrng();
                if (Misc::Rng::roll0to99(prng) >= successChance)
                {
                    if (mCaster == getPlayer())
                        MWBase::Environment::get().getWindowManager()->messageBox("#{sMagicSkillFail}");
                    fail = true;
                }
                /*
                    End of tes3mp change (major)
                */

                if (fail)
                {
                    // Failure sound
                    MWBase::SoundManager* sndMgr = MWBase::Environment::get().getSoundManager();
                    const ESM::Skill* skill = MWBase::Environment::get().getESMStore()->get<ESM::Skill>().find(school);
                    sndMgr->playSound3D(mCaster, skill->mSchool->mFailureSound, 1.0f, 1.0f);
                    static const std::string schools[] = {
                        "alteration", "conjuration", "destruction", "illusion", "mysticism", "restoration"
                    };

                    MWBase::SoundManager *sndMgr = MWBase::Environment::get().getSoundManager();
                    sndMgr->playSound3D(mCaster, "Spell Failure " + schools[school], 1.0f, 1.0f);

                    /*
                        Start of tes3mp addition

                        Send an ID_OBJECT_SOUND packet every time a sound is made here
                    */
                    mwmp::ObjectList* objectList = mwmp::Main::get().getNetworking()->getObjectList();
                    objectList->reset();
                    objectList->packetOrigin = mwmp::CLIENT_GAMEPLAY;
                    objectList->addObjectSound(mCaster, "Spell Failure " + schools[school], 1.0f, 1.0f);
                    objectList->sendObjectSound();
                    /*
                        End of tes3mp addition
                    */

                    return false;
                }
            }

            // A power can be used once per 24h
            if (spell->mData.mType == ESM::Spell::ST_Power)
                stats.getSpells().usePower(spell);
        }

        if (!mScriptedSpell && mCaster == getPlayer() && spellIncreasesSkill(spell))
            mCaster.getClass().skillUsageSucceeded(mCaster, school, ESM::Skill::Spellcast_Success);

        // A non-actor doesn't play its spell cast effects from a character controller, so play them here
        if (!mCaster.getClass().isActor())
            playSpellCastingEffects(spell->mEffects.mList);

        inflict(mCaster, spell->mEffects, ESM::RT_Self);

        if (!mTarget.isEmpty())
            inflict(mTarget, spell->mEffects, ESM::RT_Touch);

        launchMagicBolt();

        return true;
    }

    bool CastSpell::cast(const ESM::Ingredient* ingredient)
    {
        mId = ingredient->mId;
        mFlags = static_cast<ESM::ActiveSpells::Flags>(
            ESM::ActiveSpells::Flag_Temporary | ESM::ActiveSpells::Flag_Stackable);
        mSourceName = ingredient->mName;

        std::optional<ESM::EffectList> effect = rollIngredientEffect(mCaster, ingredient, 0);

        if (effect)
        {
            inflict(mCaster, *effect, ESM::RT_Self);
            return true;
        }

        if (mCaster == getPlayer())
        {
            // "X has no effect on you"
            std::string message = MWBase::Environment::get()
                                      .getESMStore()
                                      ->get<ESM::GameSetting>()
                                      .find("sNotifyMessage50")
                                      ->mValue.getString();
            message = Misc::StringUtils::format(message, ingredient->mName);
            MWBase::Environment::get().getWindowManager()->messageBox(message);
        }

        return false;
    }

    void CastSpell::playSpellCastingEffects(const ESM::Enchantment* enchantment) const
    {
        playSpellCastingEffects(enchantment->mEffects.mList);
    }

    void CastSpell::playSpellCastingEffects(const ESM::Spell* spell) const
    {
        playSpellCastingEffects(spell->mEffects.mList);
    }

    void CastSpell::playSpellCastingEffects(const std::vector<ESM::IndexedENAMstruct>& effects) const
    {
        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
        std::vector<VFS::Path::Normalized> addedEffects;

        for (const ESM::IndexedENAMstruct& effectData : effects)
        {
            const auto effect = store.get<ESM::MagicEffect>().find(effectData.mData.mEffectID);

            const ESM::Static* castStatic;

            if (!effect->mCasting.empty())
                castStatic = store.get<ESM::Static>().find(effect->mCasting);
            else
                castStatic = store.get<ESM::Static>().find(ESM::RefId::stringRefId("VFX_DefaultCast"));

            VFS::Path::Normalized castStaticModel
                = Misc::ResourceHelpers::correctMeshPath(castStatic->mModel.getNormalized());

            // check if the effect was already added
            if (std::find(addedEffects.begin(), addedEffects.end(), castStaticModel) != addedEffects.end())
                continue;

            MWRender::Animation* animation = MWBase::Environment::get().getWorld()->getAnimation(mCaster);
            if (animation)
            {
                animation->addEffect(
                    castStaticModel.value(), effect->mId.getRefIdString(), false, {}, effect->mParticle.getOriginal());
            }
            else
            {
                // If the caster has no animation, add the effect directly to the effectManager
                // We must scale and position it manually
                float scale = mCaster.getCellRef().getScale();
                osg::Vec3f pos(mCaster.getRefData().getPosition().asVec3());
                if (!mCaster.getClass().isNpc())
                {
                    osg::Vec3f bounds(MWBase::Environment::get().getWorld()->getHalfExtents(mCaster) * 2.f);
                    scale *= std::max({ bounds.x(), bounds.y(), bounds.z() / 2.f }) / 64.f;
                    float offset = 0.f;
                    if (bounds.z() < 128.f)
                        offset = bounds.z() - 128.f;
                    else if (bounds.z() < bounds.x() + bounds.y())
                        offset = 128.f - bounds.z();
                    if (MWBase::Environment::get().getWorld()->isFlying(mCaster))
                        offset /= 20.f;
                    pos.z() += offset * scale;
                }
                else
                {
                    // Additionally use the NPC's height
                    osg::Vec3f npcScaleVec(1.f, 1.f, 1.f);
                    mCaster.getClass().adjustScale(mCaster, npcScaleVec, true);
                    scale *= npcScaleVec.z();
                }
                scale = std::max(scale, 1.f);
                MWBase::Environment::get().getWorld()->spawnEffect(
                    castStaticModel, effect->mParticle.getOriginal(), pos, scale);
            }

            if (animation && !mCaster.getClass().isActor())
                animation->addSpellCastGlow(effect->getColor());

            addedEffects.push_back(std::move(castStaticModel));

            MWBase::SoundManager* sndMgr = MWBase::Environment::get().getSoundManager();
            if (!effect->mCastSound.empty())
                sndMgr->playSound3D(mCaster, effect->mCastSound, 1.0f, 1.0f);
            else
                sndMgr->playSound3D(
                    mCaster, store.get<ESM::Skill>().find(effect->mData.mSchool)->mSchool->mCastSound, 1.0f, 1.0f);
        }
    }

    void playEffects(const MWWorld::Ptr& target, const ESM::MagicEffect& magicEffect, bool playNonLooping)
    {
        const auto& store = MWBase::Environment::get().getESMStore();
        if (playNonLooping)
        {
            MWBase::SoundManager* sndMgr = MWBase::Environment::get().getSoundManager();
            if (!magicEffect.mHitSound.empty())
                sndMgr->playSound3D(target, magicEffect.mHitSound, 1.0f, 1.0f);
            else
                sndMgr->playSound3D(
                    target, store->get<ESM::Skill>().find(magicEffect.mData.mSchool)->mSchool->mHitSound, 1.0f, 1.0f);
        }

        // Add VFX
        const ESM::Static* castStatic;
        if (!magicEffect.mHit.empty())
            castStatic = store->get<ESM::Static>().find(magicEffect.mHit);
        else
            castStatic = store->get<ESM::Static>().find(ESM::RefId::stringRefId("VFX_DefaultHit"));

        bool loop = (magicEffect.mData.mFlags & ESM::MagicEffect::ContinuousVfx) != 0;
        MWRender::Animation* anim = MWBase::Environment::get().getWorld()->getAnimation(target);
        if (anim && !castStatic->mModel.empty())
        {
            // Don't play particle VFX unless the effect is new or it should be looping.
            if (playNonLooping || loop)
            {
                const VFS::Path::Normalized castStaticModel
                    = Misc::ResourceHelpers::correctMeshPath(castStatic->mModel.getNormalized());
                anim->addEffect(castStaticModel.value(), magicEffect.mId.getRefIdString(), loop, {},
                    magicEffect.mParticle.getOriginal());
            }
        }
    }
}
