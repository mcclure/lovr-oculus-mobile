/************************************************************************************

Filename    :   UIKeyboard.h
Content     :
Created     :	11/3/2015
Authors     :   Eric Duhon

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "UIKeyboard.h"

#include "Appl.h"
#include "PackageFiles.h"
#include "OVR_FileSys.h"

#include "OVR_JSON.h"

#include "UIMenu.h"
#include "UITexture.h"
#include "UIButton.h"

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <locale>

using OVR::Matrix4f;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;
using OVR::Bounds3f;
using OVR::Quatf;
using OVR::Posef;
using OVR::JSON;
using OVR::JsonReader;

namespace OVRFW
{

class UIKeyboardLocal : public UIKeyboard
{
public:
	UIKeyboardLocal( OvrGuiSys &GuiSys, UIMenu * menu );

	~UIKeyboardLocal();

	void SetKeyPressHandler( KeyPressEventT eventHandler, void * userData ) override;
	bool OnKeyEvent( const int keyCode, const int repeatCount ) override;
	
	bool Init( OvrGuiSys &GuiSys, const char * keyboardSet, const Vector3f & positionOffset );

private:
	typedef void( *command_t ) ();

	static const char * CommandShift;
	static const char * CommandChangeKeyboard;
	static const char * CommandBackspace;
	static const char * CommandReturn;

	struct Key
	{
		Key( OvrGuiSys &GuiSys ) : Button( GuiSys )
		{
		}

		std::string DisplayCharacter;
		std::string DisplayShiftCharacter;
		std::string Action;
		std::string ActionShift;
		std::string ActionArgument;
		std::string Texture;
		std::string TexturePressed;
		float Width { 0 }; // percent
		float FontScale { 1 };
		bool Toggle { false };
		UIButton Button; // UIObjects are not safe to copy after they are inited
		UIKeyboardLocal * Parent { nullptr }; // so we can speed things up a bit on button callbacks.
	};

	struct KbdRow
	{
		float Height { 10 }; // percent
		float Offset { 0 };
		std::vector<Key> Keys;
	};
	struct Keyboard
	{
		std::string Name;
		std::vector<KbdRow> KeyRows;
		const UIButton * ShiftButton { nullptr };

		std::unordered_map<std::string, UITexture> TextureTable;

		UITexture * DefaultTexture { nullptr };
		UITexture * DefaultPressedTexture { nullptr };
		Vector2f Dimensions { 1.0f, 0.5f };
	};

	void OnClick( const Key * key );
	void UpdateButtonText( const bool shiftState );
	bool BuildKeyboardData( OvrGuiSys & GuiSys, const char * keyboardPath, Keyboard * keyboard );
	bool BuildKeyboardUI( Keyboard * keyboard, const Vector3f & positionOffset );
	void LoadKeyboard( OvrGuiSys & GuiSys, const char * keyboardPath, const Vector3f & positionOffset );
	void SwitchToKeyboard( const std::string & name );

	UIMenu * Menu { nullptr };

	//storing pointers, UI classes don't like to be copied.
	std::unordered_map<std::string, Keyboard *> Keyboards;
	Keyboard * CurrentKeyboard { nullptr };
	KeyPressEventT EventHandler;
	void * UserData { nullptr };
	bool PhysicalShiftDown[2];
};

const char * UIKeyboardLocal::CommandShift = "cmd_shift";
const char * UIKeyboardLocal::CommandChangeKeyboard = "cmd_change_keyboard";
const char * UIKeyboardLocal::CommandBackspace = "cmd_backspace";
const char * UIKeyboardLocal::CommandReturn = "cmd_return";

UIKeyboardLocal::UIKeyboardLocal( OvrGuiSys & GuiSys, UIMenu * menu ) : Menu( menu )
{
	PhysicalShiftDown[0] = PhysicalShiftDown[1] = false;
}

UIKeyboardLocal::~UIKeyboardLocal()
{
	for ( auto iter = Keyboards.begin(); iter != Keyboards.end(); ++iter )
	{
		delete ( iter->second );
	}
}

bool UIKeyboardLocal::BuildKeyboardData( OvrGuiSys & GuiSys, const char * keyboardPath,
										 Keyboard * keyboard )
{
	std::vector< uint8_t > buffer;
	if ( !GuiSys.GetFileSys().ReadFile( keyboardPath, buffer ) )
	{
		assert( false );
		return false;
	}

	auto kbdJson = OVR::JSON::Parse( reinterpret_cast< char * >( static_cast< uint8_t * >( buffer.data() ) ) );
	assert( kbdJson );
	if ( !kbdJson )
	{
		return false;
	}

	const JsonReader kbdJsonReader( kbdJson );
	assert( kbdJsonReader.IsObject() );
	if ( !kbdJsonReader.IsObject() )
	{
		return false;
	}

	keyboard->Name = kbdJsonReader.GetChildStringByName( "name", "" );
	assert( !keyboard->Name.empty() ); //keyboard has to have a name
	if ( keyboard->Name.empty() )
	{
		return false;
	}

	keyboard->Dimensions.x = kbdJsonReader.GetChildFloatByName( "default_width", 1.0f );
	keyboard->Dimensions.y = kbdJsonReader.GetChildFloatByName( "default_height", 0.5f );

	const JsonReader textures( kbdJsonReader.GetChildByName( "textures" ) );
	assert( textures.IsArray() );
	if ( textures.IsArray() )
	{
		//UIObjects aren't copyable, and don't implement move semantics, and in either case
		//our containers don't support move only types anyway. So take the pain
		struct TextureStringPair
		{
			std::string Name;
			std::string Path;
		};
		std::vector<TextureStringPair> TextureData;
		while ( !textures.IsEndOfArray() )
		{
			const JsonReader texture( textures.GetNextArrayElement() );
			assert( texture.IsObject() );
			if ( texture.IsObject() )
			{
				std::string name = texture.GetChildStringByName( "name" );
				std::string texturePath = texture.GetChildStringByName( "texture" );
				assert( !texturePath.empty() && !name.empty() );
				if ( !texturePath.empty() && !name.empty() )
				{
					std::string lowerCaseS = name.c_str();
					auto & loc = std::use_facet<std::ctype<char>>( std::locale() );
					loc.tolower( &lowerCaseS[0], &lowerCaseS[0] + lowerCaseS.length() );

					keyboard->TextureTable[lowerCaseS] = UITexture();
					TextureData.push_back( TextureStringPair { name, texturePath } );
				}
			}
		}
		for ( const auto &tex : TextureData )
		{
			std::vector< uint8_t > texBuffer;
			if ( !GuiSys.GetFileSys().ReadFile( tex.Path.c_str(), texBuffer ) )
			{
				assert( false );
				return false;
			}

			std::string lowerCaseS = tex.Name.c_str();
			auto & loc = std::use_facet<std::ctype<char>>( std::locale() );
			loc.tolower( &lowerCaseS[0], &lowerCaseS[0] + lowerCaseS.length() );
			keyboard->TextureTable[lowerCaseS].LoadTextureFromBuffer(
				tex.Path.c_str(), texBuffer );
		}
	}
	{
		std::string defTexture = kbdJsonReader.GetChildStringByName( "default_texture", "" );
		std::string defPresTexture = kbdJsonReader.GetChildStringByName( "default_pressed_texture", "" );

		assert( !defTexture.empty() );
		if ( defTexture.empty() )
		{
			return false;
		}

		auto & loc = std::use_facet<std::ctype<char>>( std::locale() );
		loc.tolower( &defTexture[0], &defTexture[0] + defTexture.length() );
		loc.tolower( &defPresTexture[0], &defPresTexture[0] + defPresTexture.length() );

		if ( keyboard->TextureTable.find( defTexture ) == keyboard->TextureTable.end()  )
		{
			return false;
		}
		keyboard->DefaultTexture = &( keyboard->TextureTable[ defTexture ] );
		if ( keyboard->TextureTable.find( defPresTexture ) == keyboard->TextureTable.end() )
		{
			keyboard->DefaultPressedTexture = keyboard->DefaultTexture;
		}
		else
		{
			keyboard->DefaultPressedTexture = &( keyboard->TextureTable[ defPresTexture ] );
		}
	}

	const JsonReader rows( kbdJsonReader.GetChildByName( "rows" ) );
	assert( rows.IsArray() );
	if ( rows.IsArray() )
	{
		while ( !rows.IsEndOfArray() )
		{
			const JsonReader row( rows.GetNextArrayElement() );
			assert( row.IsObject() );
			if ( row.IsObject() )
			{
				const float offset = row.GetChildFloatByName( "offset", 0.0f ) / 100.0f;
				const float height = row.GetChildFloatByName( "height", 25.0f ) / 100.0f;
				const JsonReader keys( row.GetChildByName( "keys" ) );
				assert( keys.IsArray() );
				if ( keys.IsArray() )
				{
					keyboard->KeyRows.push_back( KbdRow() );
					KbdRow &keyRow = keyboard->KeyRows.back();
					keyRow.Height = height;
					keyRow.Offset = offset;
					while ( !keys.IsEndOfArray() )
					{
						const JsonReader key( keys.GetNextArrayElement() );
						keyRow.Keys.push_back( Key( GuiSys ) );
						Key &akey = keyRow.Keys.back();
						akey.Parent = this;

						akey.Width = key.GetChildFloatByName( "width", 10.0f ) / 100.0f;
						akey.FontScale = key.GetChildFloatByName( "font_scale", 1.0f );
						akey.Toggle = key.GetChildBoolByName( "toggle", false );

						//read order important for proper chain of defaults
						akey.DisplayCharacter = key.GetChildStringByName( "display_character",
																		  " " );
						akey.DisplayShiftCharacter = key.GetChildStringByName(
							"display_shift_character", akey.DisplayCharacter.c_str() );
						akey.Action = key.GetChildStringByName( "action",
																akey.DisplayCharacter.c_str() );
						akey.ActionShift = key.GetChildStringByName( "action_shift",
																	 akey.DisplayShiftCharacter.c_str() );
						akey.ActionArgument = key.GetChildStringByName( "action_argument", "" );
						akey.Texture = key.GetChildStringByName( "texture", "" );
						akey.TexturePressed = key.GetChildStringByName( "texture_pressed", "" );
					}
				}
			}
		}
	}

	return true;
}

bool UIKeyboardLocal::BuildKeyboardUI( Keyboard *keyboard, const Vector3f & positionOffset )
{
	Vector3f UpperLeft = Vector3f { -keyboard->Dimensions.x / 2, keyboard->Dimensions.y / 2, 0.0f } + positionOffset;
	Vector3f xOffset = UpperLeft;
	for ( auto &keyRow : keyboard->KeyRows )
	{
		xOffset.x = UpperLeft.x + ( keyboard->Dimensions.x * keyRow.Offset );
		for ( auto &key : keyRow.Keys )
		{
			UIButton &button = key.Button;
			button.AddToMenu( Menu );
			UITexture *texture = keyboard->DefaultTexture;
			UITexture *texturePressed = keyboard->DefaultPressedTexture;
			if ( !key.Texture.empty() )
			{
				std::string lowerCaseS = key.Texture.c_str();
				auto & loc = std::use_facet<std::ctype<char>>( std::locale() );
				loc.tolower( &lowerCaseS[0], &lowerCaseS[0] + lowerCaseS.length() );
				auto it = keyboard->TextureTable.find( lowerCaseS );
				if ( it != keyboard->TextureTable.end() )
				{
					texture = &( it->second );
				}
			}
			if ( !key.TexturePressed.empty() )
			{
				std::string lowerCaseS = key.TexturePressed.c_str();
				auto & loc = std::use_facet<std::ctype<char>>( std::locale() );
				loc.tolower( &lowerCaseS[0], &lowerCaseS[0] + lowerCaseS.length() );
				auto it = keyboard->TextureTable.find( lowerCaseS );
				if ( it != keyboard->TextureTable.end() )
				{
					texture = &( it->second );
				}
			}

			button.SetButtonImages( *texture, *texture, *texturePressed );
			button.SetButtonColors( Vector4f( 1, 1, 1, 1 ), Vector4f( 0.5f, 1, 1, 1 ),
									Vector4f( 1, 1, 1, 1 ) );
			button.SetLocalScale( Vector3f(
				( UIObject::TEXELS_PER_METER / texture->Width ) * key.Width *
				keyboard->Dimensions.x,
				( UIObject::TEXELS_PER_METER / texture->Height ) * keyboard->Dimensions.y *
				keyRow.Height, 1.0f ) );
			button.SetVisible( false );
			button.SetLocalPosition(
				xOffset + Vector3f( key.Width * keyboard->Dimensions.x / 2.0f, 0, 0 ) );
			button.SetText( key.DisplayCharacter.c_str() );
			if ( key.Toggle )
				button.SetAsToggleButton( *texturePressed, Vector4f( 0.5f, 1, 1, 1 ) );
			//Tend to move gaze fairly quickly when using onscreen keyboard, causes a lot of mistypes when keyboard action is on keyup instead of keydown because of this.
			button.SetActionType( UIButton::eButtonActionType::ClickOnDown );

			button.SetOnClick( [] ( UIButton *button, void *keyClicked )
			{
				Key * key = reinterpret_cast< Key * >( keyClicked );
				key->Parent->OnClick( key );
			}, &key );

			button.SetTouchDownSnd( "sv_deselect" );
			if ( key.Toggle )
			{
				button.SetTouchUpSnd( "" );
			}
			else
			{
				button.SetTouchUpSnd( "sv_select" );
			}

			VRMenuObject * menuObject = button.GetMenuObject();
			VRMenuFontParms fontParms = menuObject->GetFontParms();
			fontParms.Scale = key.FontScale;
			menuObject->SetFontParms( fontParms );
			
			if ( key.Action == CommandShift )
			{
				assert(
					!keyboard->ShiftButton ); //We only support a single shift key on a keyboard for now
				keyboard->ShiftButton = &key.Button;
			}

			xOffset.x += key.Width * keyboard->Dimensions.x;
		}
		xOffset.y -= keyboard->Dimensions.y * keyRow.Height;
	}
	return true;
}

void UIKeyboardLocal::LoadKeyboard( OvrGuiSys &GuiSys, const char *keyboardPath, const Vector3f & positionOffset )
{
	Keyboard *keyboard = new Keyboard();
	if ( !BuildKeyboardData( GuiSys, keyboardPath, keyboard ) )
	{
		delete keyboard;
		return;
	}

	if ( !BuildKeyboardUI( keyboard, positionOffset ) )
	{
		delete keyboard;
		return;
	}

	std::string lowerCaseS = keyboard->Name.c_str();
	auto & loc = std::use_facet<std::ctype<char>>( std::locale() );
	loc.tolower( &lowerCaseS[0], &lowerCaseS[0] + lowerCaseS.length() );
	auto it = Keyboards.find( lowerCaseS );
	if ( it != Keyboards.end() )
	{
		delete keyboard;
	}

	Keyboards[ lowerCaseS ] = keyboard;
	if ( !CurrentKeyboard )
		SwitchToKeyboard( keyboard->Name );
}

bool UIKeyboardLocal::Init( OvrGuiSys &GuiSys, const char * keyboardSet, const Vector3f & positionOffset )
{
	std::vector< uint8_t > buffer;
	if ( !GuiSys.GetFileSys().ReadFile( keyboardSet, buffer ) )
	{
		assert( false );
		return false;
	}

	auto kbdSetJson = JSON::Parse( reinterpret_cast< char * >( static_cast< uint8_t * >( buffer.data() ) ) );
	assert( kbdSetJson );
	if ( !kbdSetJson )
		return false;

	const JsonReader kbdSetJsonReader( kbdSetJson );
	assert( kbdSetJsonReader.IsArray() );
	if ( !kbdSetJsonReader.IsArray() )
	{
		return false;
	}

	while ( !kbdSetJsonReader.IsEndOfArray() )
	{
		std::string keyboardPath = kbdSetJsonReader.GetNextArrayString();
		assert( !keyboardPath.empty() );
		LoadKeyboard( GuiSys, keyboardPath.c_str(), positionOffset );
	}

	if ( !CurrentKeyboard ) //didn't get even one keyboard loaded
	{
		return false;
	}

	return true;
}

void UIKeyboardLocal::OnClick( const Key * key )
{
	assert( CurrentKeyboard );
	bool shiftState = false;
	if ( CurrentKeyboard->ShiftButton )
		shiftState = CurrentKeyboard->ShiftButton->IsPressed();

	const std::string * action = &key->Action;
	if ( shiftState && !key->ActionShift.empty() )
		action = &key->ActionShift;

	if ( *action == CommandShift )
	{
		UpdateButtonText( shiftState );
	}
	else if ( *action == CommandChangeKeyboard )
	{
		SwitchToKeyboard( key->ActionArgument.c_str() );
	}
	else if ( EventHandler )
	{
		if ( *action == CommandBackspace )
			EventHandler( KeyEventType::Backspace, std::string(), UserData );
		else if ( *action == CommandReturn )
			EventHandler( KeyEventType::Return, std::string(), UserData );
		else
			EventHandler( KeyEventType::Text, *action, UserData );
	}
}

void UIKeyboardLocal::UpdateButtonText( const bool shiftState )
{
	assert( CurrentKeyboard );
	if ( !CurrentKeyboard )
	{
		return;
	}

	for ( auto &keyRow : CurrentKeyboard->KeyRows )
	{
		for ( auto &key : keyRow.Keys )
		{
			if ( shiftState && !key.DisplayShiftCharacter.empty() )
				key.Button.SetText( key.DisplayShiftCharacter.c_str() );
			else
				key.Button.SetText( key.DisplayCharacter.c_str() );
		}
	}
}

void UIKeyboardLocal::SwitchToKeyboard( const std::string& name )
{
	std::string lowerCaseS = name.c_str();
	auto & loc = std::use_facet<std::ctype<char>>( std::locale() );
	loc.tolower( &lowerCaseS[0], &lowerCaseS[0] + lowerCaseS.length() );

	auto it = Keyboards.find( lowerCaseS );
	if ( it == Keyboards.end() )
	{
		OVR_LOG( "UIKeyboardLocal::SwitchToKeyboard ... could not find = '%s' !", lowerCaseS.c_str() );
		assert( false );
		return;
	}

	if ( CurrentKeyboard )
	{
		for ( auto &keyRow : CurrentKeyboard->KeyRows )
		{
			for ( auto &key : keyRow.Keys )
			{
				key.Button.SetVisible( false );
			}
		}
	}
	CurrentKeyboard = it->second;
	assert( CurrentKeyboard );
	for ( auto &keyRow : CurrentKeyboard->KeyRows )
	{
		for ( auto &key : keyRow.Keys )
		{
			key.Button.SetVisible( true );
		}
	}
}

void UIKeyboardLocal::SetKeyPressHandler( KeyPressEventT eventHandler, void * userData )
{
	EventHandler = eventHandler;
	UserData = userData;
}

bool UIKeyboardLocal::OnKeyEvent( const int keyCode, const int repeatCount )
{
	if ( keyCode == AKEYCODE_SHIFT_LEFT )
	{
		PhysicalShiftDown[0] = !PhysicalShiftDown[0]; // eventType == KEY_EVENT_DOWN;
		return false;
	}
	if ( keyCode == AKEYCODE_SHIFT_RIGHT )
	{
		PhysicalShiftDown[1] = !PhysicalShiftDown[1]; //  eventType == KEY_EVENT_DOWN;
		return false;
	}

	///if ( eventType == KEY_EVENT_DOWN )
	{
		if ( keyCode == AKEYCODE_DEL )
		{
			EventHandler( KeyEventType::Backspace, std::string(), UserData );
			return true;
		}
		else if ( keyCode == AKEYCODE_ENTER )
		{
			EventHandler( KeyEventType::Return, std::string(), UserData );
			return true;
		}
		/// TODO
		/*
		char ascii = GetAsciiForKeyCode( ( ovrKeyCode )keyCode, PhysicalShiftDown[0] || PhysicalShiftDown[1] );
		if ( ascii )
		{
			std::string action;
			action += ascii;
			EventHandler( KeyEventType::Text, action, UserData );
			return true;
		}
		*/
	}

	return false;
}

UIKeyboard * UIKeyboard::Create( OvrGuiSys &GuiSys, UIMenu * menu, const Vector3f & positionOffset )
{
	return Create( GuiSys, menu, "apk:///res/raw/default_keyboard_set.json", positionOffset );
}

UIKeyboard * UIKeyboard::Create( OvrGuiSys &GuiSys, UIMenu * menu, const char * keyboardSet, const Vector3f & positionOffset )
{
	UIKeyboardLocal * keyboard = new UIKeyboardLocal( GuiSys, menu );
	if ( !keyboard->Init( GuiSys, keyboardSet, positionOffset ) )
	{
		delete keyboard;
		return nullptr;
	}
	return keyboard;
}

void UIKeyboard::Free( UIKeyboard * keyboard )
{
	delete keyboard;
}

} // OVRFW
