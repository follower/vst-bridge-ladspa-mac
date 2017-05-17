/*
  LADSPA / VST bridge

  Copyright (C) 2002 Kjetil S. Matheussen / Notam
  Copyright (C) 2003 Dominic Mazzoni
  Copyright (C) 2006 Leland Lucius

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  As an exception to the GPL, you may distribute binary copies of
  this library that link to the VST SDK.
*/

#define VST_FORCE_DEPRECATED 0

#include <aeffect.h>
#include <aeffectx.h>
#include <ladspa.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if defined( WIN32 )
   #include <windows.h>
   #include <shlwapi.h>

   #define PATHLEN MAX_PATH
   #define _WINDOWS_DLL_EXPORT_ __declspec(dllexport)
#else
   #include <glob.h>
   #include <unistd.h>
   #include <dlfcn.h>
   #include <sys/param.h>

   #define PATHLEN PATH_MAX
   #define _WINDOWS_DLL_EXPORT_ 
#endif

#if defined( __APPLE__ )
   #include <sys/stat.h>
   #include <CoreServices/CoreServices.h>
   #include <mach-o/dyld.h>
#endif

#define DEBUG 0

#if DEBUG

#if defined( WIN32 )
#else

#include <stdio.h>
#include <stdarg.h>
FILE *ff = NULL;
void
say( char * str, ...)
{
   va_list vl;
   va_start( vl, str );

   if( !ff )
      ff = fopen( "/tmp/out", "w+" );
   if( ff )
   {
      vfprintf( ff, str, vl );
      fflush( ff );
   }

   va_end( vl );
}
#endif

#else
#define say(a,...)
#endif

typedef AEffect *( *vstPluginMain )( audioMasterCallback audioMaster );

class Module;

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
class Plug
{
 public:
   Plug();
   ~Plug();

   bool Init( unsigned long id, char *name, AEffect *effect, AEffect *effectPtr );
   char *GetStr( VstInt32 opcode, char *dflt );
   Plug *GetPrev();
   void SetPrev( Plug *prev );
   LADSPA_Descriptor *GetDescriptor();
   LADSPA_Handle Instantiate( unsigned long rate );
   
 private:
   Plug *mPrev;
   AEffect *mThunk;
   AEffect *mEffect;
   char *mName;
   void *mLib;
   LADSPA_Descriptor mDesc;
};

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
class Instance
{
 public:
   Instance();
   ~Instance();
 
   bool Init( char *name, unsigned long rate );
   void Cleanup();

   void Connect_Port( unsigned long port, LADSPA_Data *data );
   void Run( unsigned long count );
   void Run_Adding( unsigned long count );
   void Set_Run_Adding_Gain( LADSPA_Data gain );
   void Set_Parameters();
   
 private:
   Module *mModule;
   AEffect *mThunk;
   AEffect *mEffect;
   LADSPA_Data **mPorts;
   LADSPA_Data mGain;
   unsigned long mBlockSize;
   int mNumPorts;
   int mFirstParam;
};

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

static Plug *plugs = NULL;
static int numPlugs = 0;
static void *thisLib = NULL;

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

extern "C"
{
   static VstIntPtr audioMaster( AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
   {
      switch (opcode)
      {
         case audioMasterVersion:
            return 2;

         case audioMasterCurrentId:
            return 1;
      }

      return 0;
   }
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

#if defined( WIN32 )

class FindModules
{
 public:

   FindModules( char *dir = NULL )
   {
      mDir[ 0 ] = '\0';
      mHandle = INVALID_HANDLE_VALUE;
   }

   ~FindModules()
   {
      if( mHandle != INVALID_HANDLE_VALUE )
      {
         FindClose( mHandle );
      }
   }

   void Init( char *dir = NULL )
   {
      if( mHandle != INVALID_HANDLE_VALUE )
      {
         FindClose( mHandle );
         mHandle = INVALID_HANDLE_VALUE;
      }

      if( dir == NULL )
      {
         GetCurrentDirectory( PATHLEN, mDir );
      }
      else
      {
         strncpy( mDir, dir, PATHLEN );
      }
   }

   char *Next()
   {
      vstPluginMain main = NULL;
      
      if( mHandle == INVALID_HANDLE_VALUE )
      {
         _snprintf( mName, PATHLEN, "%s\\*.dll", mDir );
         mHandle = FindFirstFile( mName, &mData );
         if( mHandle == INVALID_HANDLE_VALUE )
         {
            return NULL;
         }
      }
      else
      {
         if( FindNextFile( mHandle, &mData ) == 0 )
         {
            return NULL;
         }
      }

      _snprintf( mName, PATHLEN, "%s\\%s", mDir, mData.cFileName );

      return mName;
   }

 private:

   WIN32_FIND_DATA mData;
   HANDLE mHandle;

   char mDir[ PATHLEN ];
   char mName[ PATHLEN ];
   bool mIsOpen;
};

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class Module
{
 public:

   Module()
   {
      mLib = NULL;
      mEffect = NULL;
   };

   ~Module()
   {
      if( mLib )
      {
         FreeLibrary( mLib );
      }
   };

   bool Init( char *name )
   {
      vstPluginMain entry = NULL;

      mLib = LoadLibrary( name );
      if( mLib )
      {
         entry = (vstPluginMain) GetProcAddress( mLib, "VSTPluginMain" );
         if( !entry )
         {
            entry = (vstPluginMain) GetProcAddress( mLib, "main" );
         }
      }
   
      if( entry )
      {
         mEffect = entry( audioMaster );
         if( mEffect && mEffect->magic == kEffectMagic )
         {
            return true;
         }
      }

      if( mLib )
      {
         FreeLibrary( mLib );
         mLib = NULL;
      }

      return false;
   };

   AEffect *GetThunk()
   {
      return mEffect;
   }
   
   AEffect *GetEffect()
   {
      return mEffect;
   }

 private:

   AEffect *mEffect;
   HINSTANCE mLib;
   
};

/* -------------------------------------------------------------------------
||
*/
BOOL WINAPI bridge_main( HINSTANCE hDLL, DWORD rsn, LPVOID rsvd )
{
   Plug *plug;
   Plug *prev;

   if( rsn == DLL_PROCESS_DETACH )
   {
      for( plug = plugs; plug; plug = prev )
      {
         prev = plug->GetPrev();
         delete plug;
      }

      plugs = NULL;
   }

   return TRUE;
}

#else

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class FindModules
{
 public:

   FindModules( char *dir = NULL )
   {
      mIsOpen = false;
      mDir[ 0 ] = '\0';
   }

   ~FindModules()
   {
      if( mIsOpen )
      {
         globfree( &mData );
      }
   }

   void Init( char *dir = NULL )
   {
      if( mIsOpen )
      {
         globfree( &mData );
         mIsOpen = false;
      }

      if( dir == NULL )
      {
         getcwd( mDir, PATHLEN );
      }
      else
      {
         strncpy( mDir, dir, PATHLEN );
      }
   }

   char *Next()
   {
      char name[ PATHLEN ];
      char *ptr;
      int len;
      
      if( !mIsOpen )
      {
#if defined( __APPLE__ )
         snprintf( name, PATHLEN, "%s/*", mDir );
         mHandle = glob( name, GLOB_MARK | GLOB_TILDE, NULL, &mData );
         if( mHandle )
         {
            globfree( &mData );
            return NULL;
         }
#else
         snprintf( name, PATHLEN, "%s/*.so", mDir );
         mHandle = glob( name, GLOB_MARK | GLOB_TILDE, NULL, &mData );
         if( mHandle && mHandle != GLOB_NOMATCH )
         {
            globfree( &mData );
            return NULL;
         }

         snprintf( name, PATHLEN, "%s/*.vst", mDir );
         mHandle = glob( name, GLOB_MARK | GLOB_TILDE | GLOB_APPEND, NULL, &mData );
         if( mHandle )
         {
            globfree( &mData );
            return NULL;
         }
#endif

         mIsOpen = true;
      }

      while( mHandle < (int) mData.gl_pathc )
      {
         ptr = mData.gl_pathv[ mHandle++ ];
         len = strlen( ptr );

#if defined( __APPLE__ )
         if( ptr[ len - 1 ] == '/' )
         {
            snprintf( name, PATHLEN, "%s/Contents/MacOS/*", ptr );
            glob( name, GLOB_MARK | GLOB_TILDE | GLOB_APPEND, NULL, &mData );
            continue;
         }
#endif

         return ptr;
      }

      return NULL;
   }

 private:

   glob_t mData;
   int mHandle;

   char mDir[ PATHLEN ];
   bool mIsOpen;
};

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class Module
{
 public:

   Module()
   {
      mLib = NULL;
      mEffect = NULL;
#if defined( __APPLE__ )
      mRealEffect = NULL;
      mAudioMaster = (audioMasterCallback) NewCFMFromMachO( (void *) audioMaster );   
#endif
   };

   ~Module()
   {
      if( mLib )
      {
         dlclose( mLib );
      }

#if defined( __APPLE__ )
      if( mRealEffect )
      {
         CFMDestroyThunk();
      }

      if( mAudioMaster )
      {
         DisposeMachOFromCFM( (void *) mAudioMaster );
      }

      if( mResID > 0 )
      {
         CloseResFile( mResID );
      }
#endif
   };

   bool Init( char *name )
   {
      vstPluginMain entry = NULL;

      mLib = dlopen( name, RTLD_LAZY );
      if( mLib )
      {
         entry = (vstPluginMain) dlsym( mLib, "VSTPluginMain" );
         if( !entry )
         {
#if defined( __APPLE__ )
            entry = (vstPluginMain) dlsym( mLib, "main_macho" );
#else
            entry = (vstPluginMain) dlsym( mLib, "main" );
#endif
         }
      }
#if defined( __APPLE__ )
      else
      {
         return CFMLoadEffect( name );
      }
#endif
   
      if( entry )
      {
         mEffect = entry( audioMaster );
         if( mEffect && mEffect->magic == kEffectMagic )
         {
            return true;
         }
      }

      if( mLib )
      {
         dlclose( mLib );
         mLib = NULL;
      }
   
      return false;
   };

   AEffect *GetThunk()
   {
      return mEffect;
   }
   
   AEffect *GetEffect()
   {
#if defined( __APPLE__ )
      if( mRealEffect )
      {
         return mRealEffect;
      }
#endif
      return mEffect;
   }
   
#if defined( __APPLE__ )

 private:

   // MachOFunctionPointerForCFMFunctionPointer(void *cfmfp)
   //
   // Borrowed from the Apple Sample Code file "CFM_MachO_CFM.c"
   // This function allocates a block of CFM glue code which contains
   // the instructions to call CFM routines
   void *NewMachOFromCFM( void *cfmfp )
   {
      UInt32 CFMTemplate[ 6 ] = {0x3D800000, 0x618C0000, 0x800C0000,
                                 0x804C0004, 0x7C0903A6, 0x4E800420};
      UInt32 *mfp = (UInt32*) NewPtr( sizeof( CFMTemplate ) );
   
      mfp[ 0 ] = CFMTemplate[ 0 ] | ( (UInt32) cfmfp >> 16 );
      mfp[ 1 ] = CFMTemplate[ 1 ] | ( (UInt32) cfmfp & 0xFFFF );
      mfp[ 2 ] = CFMTemplate[ 2 ];
      mfp[ 3 ] = CFMTemplate[ 3 ];
      mfp[ 4 ] = CFMTemplate[ 4 ];
      mfp[ 5 ] = CFMTemplate[ 5 ];
      MakeDataExecutable( mfp, sizeof( CFMTemplate ) );
      
      return( mfp );
   };
   
   void DisposeMachOFromCFM( void *ptr )
   {
      DisposePtr( (Ptr) ptr );
   };
   
   void *NewCFMFromMachO( void *machofp )
   {
      void *result = NewPtr( 8 );
      ( (void **) result )[ 0 ] = machofp;
      ( (void **) result )[ 1 ] = result;
   
      return result;
   };
   
   void DisposeCFMFromMachO( void *ptr )
   {
      DisposePtr( (Ptr) ptr );
   };
   
   void CFMDestroyThunk()
   {
      if( mRealEffect )
      {
         DisposeMachOFromCFM( (void *) mEffect->dispatcher );
         DisposeMachOFromCFM( (void *) mEffect->process );
         DisposeMachOFromCFM( (void *) mEffect->setParameter );
         DisposeMachOFromCFM( (void *) mEffect->getParameter );
         DisposeMachOFromCFM( (void *) mEffect->processReplacing );
   
         free( mEffect );
         mRealEffect = NULL;
      }

      return;
   };
   
   void CFMCreateThunk( AEffect *effect )
   {
      mEffect = (AEffect *) malloc( sizeof( AEffect ) );
      if( mEffect == NULL )
      {
         return;
      }
      mRealEffect = effect;

      memcpy( mEffect, mEffect, sizeof( AEffect ) );
   
      mEffect->dispatcher = (AEffectDispatcherProc) NewMachOFromCFM( (void *) effect->dispatcher );
      mEffect->process = (AEffectProcessProc) NewMachOFromCFM( (void *) effect->process );
      mEffect->setParameter = (AEffectSetParameterProc) NewMachOFromCFM( (void *) effect->setParameter );
      mEffect->getParameter = (AEffectGetParameterProc) NewMachOFromCFM( (void *) effect->getParameter );
      mEffect->processReplacing = (AEffectProcessProc) NewMachOFromCFM( (void *) effect->processReplacing );

      return;
   };
   
   void Filename2FSSpec( const char *path, FSSpec *spec )
   {
      FSRef theRef;
      
      FSPathMakeRef( (const UInt8 *) path, &theRef, NULL );
      FSGetCatalogInfo( &theRef, kFSCatInfoNone, NULL, NULL, spec, NULL );
   };
   
   bool CFMLoadEffect( const char *filename )
   {
      struct stat sb;
      short cResCB;
      FSSpec spec;

      /* only try this if it's a real file (or symlink) */
      if( stat( filename, &sb ) )
      {
         return NULL;
      }
   
      if( !( ( sb.st_mode & S_IFMT ) == S_IFREG ||
             ( sb.st_mode & S_IFMT ) == S_IFLNK ) )
      {
         return NULL;
      }
   
      Filename2FSSpec( filename, &spec );

      mResID = FSpOpenResFile( &spec, fsRdPerm );
      if( mResID <= 0 )
      {
         return NULL;
      }
   
      cResCB = Count1Resources( 'aEff' );
      for( int i = 0; i < cResCB; i++ )
      {
         Handle             codeH;
         CFragConnectionID  connID;
         Ptr                mainAddr;
         Str255             errName;
         Str255             fragName;
         short              resID;
         OSType             resType;
         OSErr              err;
         
         codeH = Get1IndResource( 'aEff', short( i + 1 ) );
         if( !codeH )
         {
            continue;
         }
   
         GetResInfo( codeH, &resID, &resType, fragName );
         DetachResource( codeH );
         HLock( codeH );
   
         err = GetMemFragment( *codeH,
                               GetHandleSize( codeH ),
                               fragName,
                               kPrivateCFragCopy,
                               &connID,
                               (Ptr *) &mainAddr,
                               errName );
         if( !err )
         {
            vstPluginMain   pluginMain;
            AEffect *effect;

            pluginMain = (vstPluginMain) NewMachOFromCFM( mainAddr );
            effect = pluginMain( mAudioMaster );
            DisposeMachOFromCFM( (void *) pluginMain );
   
            if( effect && effect->magic == kEffectMagic )
            {
               CFMCreateThunk( effect );
            }
            
            break;
         }
      }
   
      if( mEffect )
      {
         return true;
      }

      CloseResFile( mResID );
      mResID = -1;
   
      return false;
   };

   short mResID;
   AEffect *mRealEffect;
   audioMasterCallback mAudioMaster;

#endif

 private:

   AEffect *mEffect;
   void *mLib;

};

/* -------------------------------------------------------------------------
||
*/
void __attribute__ ((constructor)) bridge_init(void)
{
   Dl_info di;

   // Prevent premature unloading of this library.  This can (but shouldn't)
   // occur when the Bridge dlclose()s after attempting to load itself while
   // scanning for VSTs.  I don't think this should happen since reference
   // counting should prevent it, but it does on the Mac at least, so...
   dladdr( (const void *) &ladspa_descriptor, &di );
   thisLib = dlopen( di.dli_fname, RTLD_LAZY );
}

/* -------------------------------------------------------------------------
||
*/
void __attribute__ ((destructor)) bridge_term(void)
{
   Plug *plug;
   Plug *prev;

   for( plug = plugs; plug; plug = prev )
   {
      prev = plug->GetPrev();
      delete plug;
   }

   plugs = NULL;
}

#endif

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

static void LoadEffects( char *dir )
{
   FindModules m;
   char *name;

   m.Init( dir );

   while( ( name = m.Next() ) )
   {
      Module *v;
      Plug *plug;

      v = new Module;
      if( v )
      {
         if( v->Init( name ) )
         {
            plug = new Plug;
            if( plug )
            {
               if( plug->Init( 100000 + numPlugs++, name, v->GetThunk(), v->GetEffect()  ) )
               {
                  plug->SetPrev( plugs );
                  plugs = plug;
               }
               else
               {
                  delete plug;
               }
            }
         }

         delete v;
      }
   }
}

_WINDOWS_DLL_EXPORT_
const LADSPA_Descriptor *ladspa_descriptor( unsigned long index )
{
   Plug *plug;
   unsigned long i;

   if( plugs == NULL )
   {
      char *path;

      LoadEffects( NULL );

      path = getenv( "VST_PATH" );
      if( path )
      {
         LoadEffects( path );
      }

#if defined( WIN32 )
      TCHAR dpath[ PATHLEN ];
      TCHAR tpath[ PATHLEN ];
      DWORD len = sizeof( tpath );

      dpath[ 0 ] = '\0';
      ExpandEnvironmentStrings( "%ProgramFiles%\\Steinberg\\VSTPlugins", dpath, sizeof( dpath ) );

      if( SHRegGetUSValue( "Software\\VST",
          "VSTPluginsPath",
          NULL,
          tpath,
          &len,
          FALSE,
          dpath,
          (DWORD) strlen( dpath ) ) == ERROR_SUCCESS )
      {
         tpath[ len ] = '\0';
         LoadEffects( tpath );
      }
#endif

#if defined( __APPLE__ )
      char *user = getenv("USER");
      if( user )
      {
         int len = strlen( user ) + 100;
         char *path = (char *) alloca( len );
         snprintf( path, len, "/Users/%s/Library/Audio/Plug-Ins/VST", user );
         LoadEffects( path );
      }

      LoadEffects( "/Library/Audio/Plug-Ins/VST" );
#endif
   }

   i = 0;
   for( plug = plugs; plug != NULL; plug = plug->GetPrev() )
   {
      if( i == index )
      {
         return plug->GetDescriptor();
      }

      i++;
   }

   return NULL;
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

static LADSPA_Handle instantiate( const LADSPA_Descriptor *desc, unsigned long rate )
{
   Plug *plug = (Plug *) desc->ImplementationData;

   return plug->Instantiate( rate );
}

static void cleanup(LADSPA_Handle handle)
{
   Instance *inst = (Instance *) handle;

   inst->Cleanup();

   delete inst;
}

static void connect_port( LADSPA_Handle handle, unsigned long port, LADSPA_Data *data )
{
   Instance *inst = (Instance *) handle;

   inst->Connect_Port( port, data );
}

static void run( LADSPA_Handle handle, unsigned long count )
{
   Instance *inst = (Instance *) handle;

   inst->Run( count );
}

static void run_adding( LADSPA_Handle handle, unsigned long count )
{
   Instance *inst = (Instance *) handle;

   inst->Run_Adding( count );
}

static void set_run_adding_gain( LADSPA_Handle handle, LADSPA_Data gain )
{
   Instance *inst = (Instance *) handle;

   inst->Set_Run_Adding_Gain( gain );
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
Plug::Plug()
{
   mLib = NULL;
   mPrev = NULL;
   mEffect = NULL;
   mName = NULL;
   memset( &mDesc, 0, sizeof( mDesc ) );
}

Plug::~Plug()
{
   unsigned long i;

   if( mDesc.Label )
   {
      delete mDesc.Label;
   }

   if( mDesc.Name )
   {
      delete mDesc.Name;
   }

   if( mDesc.Maker )
   {
      delete mDesc.Maker;
   }

   if( mDesc.Copyright )
   {
      delete mDesc.Copyright;
   }

   for( i = 0; i < mDesc.PortCount; i++ )
   {
      if( mDesc.PortNames )
      {
         if( mDesc.PortNames[ i ] )
         {
            free( (void *) mDesc.PortNames[ i ] );
         }
      }
   }

   if( mDesc.PortDescriptors )
   {
      delete [] mDesc.PortDescriptors;
   }
 
   if( mDesc.PortNames )
   {
      delete [] mDesc.PortNames;
   }
 
   if( mDesc.PortRangeHints )
   {
      delete [] mDesc.PortRangeHints;
   }

   if( mName )
   {
      free( mName );
   }
}

bool Plug::Init( unsigned long id, char *name, AEffect *thunk, AEffect *effect )
{
   LADSPA_PortDescriptor *portdescriptors;
   LADSPA_PortRangeHint *hints;
   char **portnames;
   char temp[ 500 ];
   int port;
   int i;
   
   mThunk = thunk;
   mEffect = effect;
   mName = strdup( name );
   if( !mName )
   {
      return false;
   }

   mDesc.UniqueID = id;

   mDesc.Label = GetStr( effGetEffectName, name );
   mDesc.Name = GetStr( effGetProductString, name );
   mDesc.Maker = GetStr( effGetVendorString, "None" );
   mDesc.Copyright = GetStr( effVendorSpecific, "None" );

   if( !mDesc.Label || !mDesc.Name || !mDesc.Maker || !mDesc.Copyright )
   {
      return false;
   }

   mDesc.PortCount = mEffect->numInputs +
                     mEffect->numOutputs +
                     mEffect->numParams;
   mDesc.PortDescriptors = portdescriptors = new LADSPA_PortDescriptor[ mDesc.PortCount ];
   mDesc.PortNames = portnames = new char *[ mDesc.PortCount ];
   mDesc.PortRangeHints = hints = new LADSPA_PortRangeHint[ mDesc.PortCount ];

   if( !mDesc.PortDescriptors || !mDesc.PortNames || !mDesc.PortRangeHints )
   {
      return false;
   }

   mDesc.ImplementationData = (void *) this;

   mDesc.instantiate = instantiate;
   mDesc.connect_port = connect_port;
   mDesc.run = run;
   mDesc.run_adding = run_adding;
   mDesc.set_run_adding_gain = set_run_adding_gain;
   mDesc.cleanup = cleanup;

   say( "name '%s'\n", mDesc.Name );
   say( "maker '%s'\n", mDesc.Maker );
   say( "label '%s'\n", mDesc.Label );
   say( "crite '%s'\n", mDesc.Copyright );

   port = 0;

   for( i = 0; i < mEffect->numInputs; i++, port++ )
   {
      portdescriptors[ port ] = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;

      sprintf( temp, "Audio In %d", i );
      portnames[ port ] = strdup( temp );

      if( !portnames[ port ] )
      {
         return false;
      }

      hints[ port ].HintDescriptor = LADSPA_HINT_DEFAULT_NONE;
   }

   for( i = 0; i < mEffect->numOutputs; i++, port++ )
   {
      portdescriptors[ port ] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;

      sprintf( temp, "Audio Out %d", i );

      portnames[ port ] = strdup( temp );
      if( !portnames[ port ] )
      {
         return false;
      }

      hints[ port ].HintDescriptor = LADSPA_HINT_DEFAULT_NONE;
   }

   for( i = 0; i < mEffect->numParams; i++, port++ )
   {
      float val;

      portdescriptors[ port ] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;

      memset( temp, 0, sizeof( temp ) );
      sprintf( temp, "_paramname_%d", i );

      mThunk->dispatcher( mEffect,
                          effGetParamName,
                          i,
                          0,
                          temp,
                          0 );
      temp[ sizeof( temp ) - 1 ] = '\0';

      portnames[ port ] = strdup( temp );
      if( !portnames[ port ] )
      {
         return false;
      }

      hints[ port ].HintDescriptor = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE;
      hints[ port ].LowerBound = 0.0f;
      hints[ port ].UpperBound = 1.0f;
      
      val = mThunk->getParameter( mEffect, i );

      if( val < 0.4f )
      {
         if( val < 0.1f )
         {
            if( val == 0.0f )
            {
               hints[ port ].HintDescriptor |= LADSPA_HINT_DEFAULT_0;
            }
            else
            {
               hints[ port ].HintDescriptor |= LADSPA_HINT_DEFAULT_MINIMUM;
            }
         }
         else
         {
            hints[ port ].HintDescriptor |= LADSPA_HINT_DEFAULT_LOW;
         }
      }
      else
      {
         if( val < 0.6f )
         {
            hints[ port ].HintDescriptor |= LADSPA_HINT_DEFAULT_MIDDLE;
         }
         else
         {
            if( val > 0.9f )
            {
               if( val == 1.0f )
               {
                  hints[ port ].HintDescriptor |= LADSPA_HINT_DEFAULT_1;
               }
               else
               {
                  hints[ port ].HintDescriptor |= LADSPA_HINT_DEFAULT_MAXIMUM;
               }
            }
            else
            {
               hints[ port ].HintDescriptor |= LADSPA_HINT_DEFAULT_HIGH;
            }
         }
      }
   }

   return true;
}

Plug *Plug::GetPrev()
{
   return mPrev;
}

void Plug::SetPrev( Plug *prev )
{
   mPrev = prev;
}

LADSPA_Descriptor *Plug::GetDescriptor()
{
   return &mDesc;
}

char *Plug::GetStr( VstInt32 opcode, char *dflt )
{
   char str[ 256 ];
   int len;
   int i;
   int dot;
   int slash;

   if( mThunk->dispatcher( mEffect, opcode, 0, 0, str, 0 ) )
   {
      if( strlen( str ) > 0 )
      {
         return strdup( str );
      }
   }

   len = (int) strlen( dflt );
   dot = -1;
   slash = -1;

   for( i = len - 1; i >= 0; i-- )
   {
      if( dot < 0 && dflt[ i ] == '.' )
      {
         dot = i;
         continue;
      }

      if( dflt[ i ] != '/' && dflt[ i ] != '\\' )
      {
         continue;
      }

      slash = i;
      break;
   }

   if( slash > 0 )
   {
      strncpy( str, &dflt[ slash + 1 ], len - slash - 1 );
      str[ len - slash - 1 ] = '\0';
   }
   else
   {
      strcpy( str, dflt );
   }

   if( dot > 0 )
   {
      str[ dot - slash - 1 ] = '\0';
   }

   return strdup( str );
}

LADSPA_Handle Plug::Instantiate( unsigned long rate )
{
   Instance *inst = new Instance;

   if( !inst->Init( mName, rate ) )
   {
      delete inst;
      inst = NULL;
   }

   return (LADSPA_Handle) inst;
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
Instance::Instance()
{
   mModule = NULL;
}

Instance::~Instance()
{
   if( mModule )
   {
      delete mModule;
   }
}

bool Instance::Init( char *name, unsigned long rate )
{
   mModule = new Module;
   if( !mModule )
   {
      return false;
   }

   if( !mModule->Init( name ) )
   {
      delete mModule;
      mModule = NULL;
      return false;
   }

   mThunk = mModule->GetThunk();
   mEffect = mModule->GetEffect();

   mBlockSize = 0;
   mFirstParam = mEffect->numInputs + mEffect->numOutputs;
   mNumPorts = mFirstParam + mEffect->numParams;
   mGain = 1.0f;

   mPorts = new LADSPA_Data *[ mNumPorts ];
   if( mPorts == NULL )
   {
      return false;
   }
   memset( mPorts, 0, mNumPorts * sizeof(LADSPA_Data *));

   mThunk->dispatcher( mEffect, effOpen, 0, 0, NULL, 0 );
   mThunk->dispatcher( mEffect, effMainsChanged, 0, 0, NULL, 0 );
   mThunk->dispatcher( mEffect, effSetSampleRate, 0, 0, NULL, (float) rate );
   mThunk->dispatcher( mEffect, effSetBlockSize, 0, 64, NULL, 0 );
   mThunk->dispatcher( mEffect, effMainsChanged, 0, 1, NULL, 0 );

   return true;
}

void Instance::Cleanup()
{
   mThunk->dispatcher( mEffect, effClose, 0, 0, NULL, 0 );

   delete [] mPorts;
}

void Instance::Connect_Port( unsigned long port, LADSPA_Data *data )
{
   mPorts[ port ] = data;
}

void Instance::Run( unsigned long count )
{
   Set_Parameters();

   if( mBlockSize != count )
   {
      mThunk->dispatcher( mEffect, effMainsChanged, 0, 0, NULL, 0);
      mThunk->dispatcher( mEffect, effSetBlockSize, 0, count, NULL, 0);
      mThunk->dispatcher( mEffect, effMainsChanged, 0, 1, NULL, 0);

      mBlockSize = count;
   }

   mThunk->processReplacing( mEffect,
                             &mPorts[ 0 ],
                             &mPorts[ mEffect->numInputs ],
                             count );
}

void Instance::Run_Adding( unsigned long count )
{
   LADSPA_Data *out[ 2 ];
   int index;
   int ich;
   int och;

   Set_Parameters();

   if( mBlockSize != count )
   {
      mThunk->dispatcher( mEffect, effMainsChanged, 0, 0, NULL, 0);
      mThunk->dispatcher( mEffect, effSetBlockSize, 0, count, NULL, 0);
      mThunk->dispatcher( mEffect, effMainsChanged, 0, 1, NULL, 0);

      mBlockSize = count;
   }

   out[ 0 ] = new LADSPA_Data[ count ];
   if( !out[ 0 ] )
   {      
      return;
   }

   out[ 1 ] = new LADSPA_Data[ count ];
   if( !out[ 0 ] || !out[ 1 ] )
   {
      delete [] out[ 0 ];
      return;
   }

   mThunk->processReplacing( mEffect,
                             &mPorts[ 0 ],
                             &out[ 0 ],
                             count );
   
   for( och = mEffect->numInputs, ich = 0; och < mEffect->numInputs + mEffect->numOutputs ; och++, ich++ )
   {
      for( index = 0; index < (int) count; index++ )
      {
         mPorts[ och ][ index ] += out[ ich ][ index ] * mGain;
      }
   }

   delete [] out[ 0 ];
   delete [] out[ 1 ];
}

void Instance::Set_Run_Adding_Gain( LADSPA_Data gain )
{
   mGain = gain;
}

void Instance::Set_Parameters()
{
   int index;

   for( index = mFirstParam; index < mNumPorts; index++ )
   {
      if( mPorts[index] )
      {
         mThunk->setParameter( mEffect,
                               index - mFirstParam,
                               *mPorts[ index ] );
      }
   }
}
