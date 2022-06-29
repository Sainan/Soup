#pragma once

#include <cstdint>

namespace soup
{
	// chess
	struct ChessCoordinate;

	// crypto
	struct Keystore;
	namespace rsa
	{
		struct Keypair;
	}

	// crypto.x509
	struct Certchain;

	// data
	struct Oid;
	template <typename T> class UniquePtr;

	// data.asn1
	struct Asn1Sequence;

	// data.json
	struct JsonArray;
	struct JsonBool;
	struct JsonFloat;
	struct JsonInt;
	struct JsonObject;
	struct JsonString;

	// io.stream
	class Reader;
	class StringReader;
	class Writer;

	// lang
	struct Block;
	struct CompilerState;
	class LangDesc;
	struct Lexeme;
	class ParserState;
	struct ParseTreeNode;
	struct Token;

	// math
	class Bigint;
	struct Vector2;
	struct Vector3;

	// math.3d
	struct BoxCorners;
	class Matrix;
	struct Mesh;
	struct Poly;
	class Quaternion;
	struct Ray;

	// mem
	struct AllocRaiiRemote;
	struct Pattern;
	struct CompiletimePatternWithOptBytesBase;
	class Pointer;
	class Range;
	struct VirtualRegion;

	// net
	class IpAddr;
	class Server;
	struct ServerService;
	class Socket;

	// net.tls
	class SocketTlsHandshaker;
	struct TlsServerRsaData;
	struct TlsClientHello;

	// net.web
	class HttpRequest;
	struct HttpResponse;

	// net.web.websocket
	struct WebSocketMessage;

	// os
	enum ControlInput : uint8_t;
	class Module;
	enum MouseButton : uint8_t;

	// os.windows
	struct HandleRaii;

	// task
	class PromiseBase;
	class Scheduler;

	// util
	class Mixed;
	struct Status;

	// vis
	class Canvas;
	struct FormattedText;
	struct RasterFont;

	// vis.render
	struct RenderTarget;

	// vis.ui.conui
	struct ConuiApp;
	struct ConuiDiv;

	// vis.ui.editor
	struct Editor;
	struct EditorText;
}
