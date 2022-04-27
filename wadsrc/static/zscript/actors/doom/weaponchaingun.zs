// --------------------------------------------------------------------------
//
// Chaingun
//
// --------------------------------------------------------------------------

class Chaingun : DoomWeapon
{
	Default
	{
		Weapon.SelectionOrder 700;
		Weapon.AmmoUse 1;
		Weapon.AmmoGive 20;
		Weapon.AmmoType "Clip";
		Inventory.PickupMessage "$GOTCHAINGUN";
		Obituary "$OB_MPCHAINGUN";
		Tag "$TAG_CHAINGUN";
	}
	States
	{
	Ready:
		CHGG A 1 A_WeaponReady;
		Loop;
	Deselect:
		CHGG A 1 A_Lower;
		Loop;
	Select:
		CHGG A 1 A_Raise;
		Loop;
	Fire:
		CHGG AB 4 A_FireCGun;
		CHGG B 0 A_ReFire;
		Goto Ready;
	Flash:
		CHGF A 5 Bright A_Light1;
		Goto LightDone;
		CHGF B 5 Bright A_Light2;
		Goto LightDone;
	Spawn:
		MGUN A -1;
		Stop;
	}
}

//===========================================================================
//
// Code (must be attached to StateProvider)
//
//===========================================================================

extend class StateProvider
{
	action void A_FireCGun()
	{
		if (player == null)
		{
			return;
		}
		int hand = 0;
		int alflags = 0;
		int snd_channel = CHAN_WEAPON;
		Weapon weap = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
		if (weap != null && invoker == weap && stateinfo != null && stateinfo.mStateType == STATE_Psprite)
		{
			hand = weap.bOffhandWeapon ? 1 : 0;
			alflags |= hand ? ALF_ISOFFHAND : 0;
			snd_channel = weap.bOffhandWeapon ? CHAN_OFFWEAPON : CHAN_WEAPON;
			if (!weap.DepleteAmmo (weap.bAltFire, true, 1))
				return;

			State flash = weap.FindState('Flash');
			if (flash != null)
			{
				// Removed most of the mess that was here in the C++ code because SetSafeFlash already does some thorough validation.
				State atk = weap.FindState('Fire');
				let psp = player.GetPSprite(hand ? PSP_OFFHANDWEAPON : PSP_WEAPON);
				if (psp) 
				{
					State cur = psp.CurState;
					int theflash = atk == cur? 0:1;
					player.SetSafeFlash(weap, flash, theflash);
				}
			}
		}
		player.mo.PlayAttacking2 ();
		A_StartSound ("weapons/chngun", snd_channel);
		GunShot (!player.refire, "BulletPuff", BulletSlope (aimflags: alflags));
	}
}
