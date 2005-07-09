/* Automatically generated using kdom/scripts/constants.pl. DO NOT EDIT ! */

#ifndef KDOM_EVENTSCONSTANTS_H
#define KDOM_EVENTSCONSTANTS_H

namespace KDOM
{
	struct MutationEventConstants
	{
		typedef enum
		{
			RelatedNode, PrevValue, NewValue, AttrName, AttrChange, 
			InitMutationEvent 
		} MutationEventConstantsEnum;
	};

	struct EventExceptionConstants
	{
		typedef enum
		{
			Code 
		} EventExceptionConstantsEnum;
	};

	struct UIEventConstants
	{
		typedef enum
		{
			View, Detail, KeyCode, InitUIEvent 
		} UIEventConstantsEnum;
	};

	struct DocumentEventConstants
	{
		typedef enum
		{
			Dummy, CreateEvent 
		} DocumentEventConstantsEnum;
	};

	struct EventConstants
	{
		typedef enum
		{
			Type, Target, CurrentTarget, EventPhase, Bubbles, 
			Cancelable, TimeStamp, StopPropagation, PreventDefault, InitEvent 
		} EventConstantsEnum;
	};

	struct EventTargetConstants
	{
		typedef enum
		{
			Dummy, AddEventListener, RemoveEventListener, DispatchEvent 
		} EventTargetConstantsEnum;
	};

	struct MouseEventConstants
	{
		typedef enum
		{
			ScreenX, ScreenY, ClientX, ClientY, CtrlKey, 
			ShiftKey, AltKey, MetaKey, Button, RelatedTarget, InitMouseEvent 
		} MouseEventConstantsEnum;
	};

	struct KeyboardEventConstants
	{
		typedef enum
		{
			KeyIdentifier, KeyLocation, CtrlKey, ShiftKey, AltKey, 
			MetaKey, GetModifierState, InitKeyboardEvent 
		} KeyboardEventConstantsEnum;
	};
};

#endif
