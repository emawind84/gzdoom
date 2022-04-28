// --------------------------------------------------------------------------
//
// Super Shotgun
//
// --------------------------------------------------------------------------

class SuperShotgun : DoomWeapon
{
	Default
	{
		Weapon.SelectionOrder 400;
		Weapon.AmmoUse 2;
		Weapon.AmmoGive 8;
		Weapon.AmmoType "Shell";
		Inventory.PickupMessage "$GOTSHOTGUN2";
		Obituary "$OB_MPSSHOTGUN";
		Tag "$TAG_SUPERSHOTGUN";
	}
	States
	{
	Ready:
		SHT2 A 1 A_WeaponReady;
		Loop;
	Deselect:
		SHT2 A 1 A_Lower;
		Loop;
	Select:
		SHT2 A 1 A_Raise;
		Loop;
	Fire:
		SHT2 A 3;
		SHT2 A 7 A_FireShotgun2;
		SHT2 B 7;
		SHT2 C 7 A_CheckReload;
		SHT2 D 7 A_OpenShotgun2;
		SHT2 E 7;
		SHT2 F 7 A_LoadShotgun2;
		SHT2 G 6;
		SHT2 H 6 A_CloseShotgun2;
		SHT2 A 5 A_ReFire;
		Goto Ready;
	// unused states
		SHT2 B 7;
		SHT2 A 3;
		Goto Deselect;
	Flash:
		SHT2 I 4 Bright A_Light1;
		SHT2 J 3 Bright A_Light2;
		Goto LightDone;
	Spawn:
		SGN2 A -1;
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
	action void A_FireShotgun2()
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
			snd_channel = weap.bOffhandWeapon ? CHAN_OFFWEAPON : CHAN_WEAPON;
			alflags |= weap.bOffhandWeapon ? ALF_ISOFFHAND : 0;
			if (!weap.DepleteAmmo (weap.bAltFire, true, 2))
				return;
			
			player.SetPsprite(PSP_FLASH, weap.FindState('Flash'), true, weap);
		}
		A_StartSound ("weapons/sshotf", snd_channel);
		player.mo.PlayAttacking2 ();

		double pitch = BulletSlope (aimflags: alflags);
			
		for (int i = 0 ; i < 20 ; i++)
		{
			int damage = 5 * random[FireSG2](1, 3);
			double ang = angle + Random2[FireSG2]() * (11.25 / 256);

			// Doom adjusts the bullet slope by shifting a random number [-255,255]
			// left 5 places. At 2048 units away, this means the vertical position
			// of the shot can deviate as much as 255 units from nominal. So using
			// some simple trigonometry, that means the vertical angle of the shot
			// can deviate by as many as ~7.097 degrees.

			LineAttack (ang, PLAYERMISSILERANGE, pitch + Random2[FireSG2]() * (7.097 / 256), damage, 'Hitscan', "BulletPuff", hand ? LAF_ISOFFHAND : 0);
		}
	}


	action void A_OpenShotgun2() 
	{ 
		int snd_channel = CHAN_WEAPON;
		if (player != null)
		{
			Weapon weap = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
			if (weap != null && invoker == weap && stateinfo != null && stateinfo.mStateType == STATE_Psprite)
			{
				snd_channel = weap.bOffhandWeapon ? CHAN_OFFWEAPON : CHAN_WEAPON;
			}

		}
		A_StartSound("weapons/sshoto", snd_channel); 
	}
	
	action void A_LoadShotgun2() 
	{ 
		int snd_channel = CHAN_WEAPON;
		if (player != null)
		{
			Weapon weap = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
			if (weap != null && invoker == weap && stateinfo != null && stateinfo.mStateType == STATE_Psprite)
			{
				snd_channel = weap.bOffhandWeapon ? CHAN_OFFWEAPON : CHAN_WEAPON;
			}

		}
		A_StartSound("weapons/sshotl", snd_channel); 
	}
	
	action void A_CloseShotgun2() 
	{ 
		int snd_channel = CHAN_WEAPON;
		if (player != null)
		{
			Weapon weap = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
			if (weap != null && invoker == weap && stateinfo != null && stateinfo.mStateType == STATE_Psprite)
			{
				snd_channel = weap.bOffhandWeapon ? CHAN_OFFWEAPON : CHAN_WEAPON;
			}

		}
		A_StartSound("weapons/sshotc", snd_channel);
		A_Refire();
	}
}
