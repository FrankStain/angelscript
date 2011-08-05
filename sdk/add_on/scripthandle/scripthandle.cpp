#include "scripthandle.h"
#include <new>
#include <assert.h>
#include <string.h>

BEGIN_AS_NAMESPACE

static void Construct(CScriptHandle *self) { new(self) CScriptHandle(); }
static void Construct(CScriptHandle *self, const CScriptHandle &o) { new(self) CScriptHandle(o); }
static void Construct(CScriptHandle *self, void *ref, int typeId) { new(self) CScriptHandle(ref, typeId); }
static void Destruct(CScriptHandle *self) { self->~CScriptHandle(); }

CScriptHandle::CScriptHandle()
{
	m_ref  = 0;
	m_type = 0;
}

CScriptHandle::CScriptHandle(const CScriptHandle &other)
{
	m_ref  = other.m_ref;
	m_type = other.m_type;

	AddRefHandle();
}

CScriptHandle::CScriptHandle(void *ref, int typeId)
{
	m_ref  = 0;
	m_type = 0;

	opAssign(ref, typeId);
}

CScriptHandle::~CScriptHandle()
{
	ReleaseHandle();
}

void CScriptHandle::ReleaseHandle()
{
	if( m_ref && m_type )
	{
		asIScriptEngine *engine = m_type->GetEngine();
		engine->ReleaseScriptObject(m_ref, m_type->GetTypeId());

		m_ref  = 0;
		m_type = 0;
	}
}

void CScriptHandle::AddRefHandle()
{
	if( m_ref && m_type )
	{
		asIScriptEngine *engine = m_type->GetEngine();
		engine->AddRefScriptObject(m_ref, m_type->GetTypeId());
	}
}

CScriptHandle &CScriptHandle::operator =(const CScriptHandle &other)
{
	// Don't do anything if it is the same reference
	if( m_ref == other.m_ref )
		return *this;

	ReleaseHandle();

	m_ref  = other.m_ref;
	m_type = other.m_type;

	AddRefHandle();

	return *this;
}

CScriptHandle &CScriptHandle::opAssign(void *ref, int typeId)
{
	ReleaseHandle();

	// When receiving a null handle we just clear our memory
	if( typeId == 0 )
		ref = 0;

	// Dereference received handles to get the object
	if( typeId & asTYPEID_OBJHANDLE )
	{
		// Store the actual reference
		ref = *(void**)ref;
		typeId &= ~asTYPEID_OBJHANDLE;
	}

	m_ref  = ref;

	// Get the object type 
	// TODO: This doesn't work when called from the application, as asGetActiveContext will return null
	asIScriptContext *ctx = asGetActiveContext();
	asIScriptEngine *engine = ctx->GetEngine();
	m_type = engine->GetObjectTypeById(typeId);

	AddRefHandle();

	return *this;
}

bool CScriptHandle::opEquals(const CScriptHandle &o) const
{
	if( m_ref  == o.m_ref &&
		m_type == o.m_type )
		return true;

	// TODO: If type is not the same, we should attempt to do a dynamic cast,
	//       which may change the pointer for application registered classes

	return false;
}

bool CScriptHandle::opEquals(void *ref, int typeId) const
{
	// Null handles are received as reference to a null handle
	if( typeId == 0 )
		ref = 0;

	// Dereference handles to get the object
	if( typeId & asTYPEID_OBJHANDLE )
	{
		// Compare the actual reference
		ref = *(void**)ref;
		typeId &= ~asTYPEID_OBJHANDLE;
	}

	// TODO: If typeId is not the same, we should attempt to do a dynamic cast, 
	//       which may change the pointer for application registered classes

	if( ref == m_ref ) return true;

	return false;
}

// AngelScript: used as '@obj = cast<obj>(ref);'
void CScriptHandle::opCast(void **outRef, int typeId)
{
	// It is expected that the outRef is always a handle
	assert( typeId & asTYPEID_OBJHANDLE );

	// Compare the type id of the actual object
	typeId &= ~asTYPEID_OBJHANDLE;

	asIScriptContext *ctx = asGetActiveContext();
	asIScriptEngine *engine = ctx->GetEngine();
	asIObjectType *type = engine->GetObjectTypeById(typeId);

	if( type != m_type )
	{
		// TODO: Should attempt a dynamic cast of the stored handle to the requested handle

		*outRef = 0;
		return;
	}

	// Must increase the ref count as we're returning a new reference to the object
	AddRefHandle();
	*outRef = m_ref;
}

/*
bool CScriptAny::Retrieve(void *ref, int refTypeId) const
{
	if( refTypeId & asTYPEID_OBJHANDLE )
	{
		// Is the handle type compatible with the stored value?

		// A handle can be retrieved if the stored type is a handle of same or compatible type
		// or if the stored type is an object that implements the interface that the handle refer to.
		if( (value.typeId & asTYPEID_MASK_OBJECT) && 
			engine->IsHandleCompatibleWithObject(value.valueObj, value.typeId, refTypeId) )
		{
			engine->AddRefScriptObject(value.valueObj, value.typeId);
			*(void**)ref = value.valueObj;

			return true;
		}
	}
}
*/

void RegisterScriptHandle_Native(asIScriptEngine *engine)
{
	int r;

	r = engine->RegisterObjectType("handle", sizeof(CScriptHandle), asOBJ_VALUE | asOBJ_ASHANDLE | asOBJ_APP_CLASS_CDAK); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("handle", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR(Construct, (CScriptHandle *), void), asCALL_CDECL_OBJFIRST); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("handle", asBEHAVE_CONSTRUCT, "void f(const handle &in)", asFUNCTIONPR(Construct, (CScriptHandle *, const CScriptHandle &), void), asCALL_CDECL_OBJFIRST); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("handle", asBEHAVE_CONSTRUCT, "void f(const ?&in)", asFUNCTIONPR(Construct, (CScriptHandle *, void *, int), void), asCALL_CDECL_OBJFIRST); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("handle", asBEHAVE_DESTRUCT, "void f()", asFUNCTIONPR(Destruct, (CScriptHandle *), void), asCALL_CDECL_OBJFIRST); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("handle", asBEHAVE_REF_CAST, "void f(?&out)", asMETHODPR(CScriptHandle, opCast, (void **, int), void), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("handle", "handle &opAssign(const handle &in)", asMETHOD(CScriptHandle, operator=), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("handle", "handle &opAssign(const ?&in)", asMETHOD(CScriptHandle, opAssign), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("handle", "bool opEquals(const handle &in) const", asMETHODPR(CScriptHandle, opEquals, (const CScriptHandle &) const, bool), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("handle", "bool opEquals(const ?&in) const", asMETHODPR(CScriptHandle, opEquals, (void*, int) const, bool), asCALL_THISCALL); assert( r >= 0 );
}

void RegisterScriptHandle(asIScriptEngine *engine)
{
//	if( strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY") )
//		RegisterScriptHandle_Generic(engine);
//	else
		RegisterScriptHandle_Native(engine);
}


END_AS_NAMESPACE
