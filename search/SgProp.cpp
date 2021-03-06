//----------------------------------------------------------------------------

//----------------------------------------------------------------------------

#include "platform/SgSystem.h"
#include "SgProp.h"

#include <iomanip>
#include <sstream>
#include "board/GoRect.h"
#include "board/SgUtil.h"
#include "lib/SgVector.h"

using namespace std;
using GoPointUtil::InBoardRange;
using SgPropUtil::PointToSgfString;
using SgPropUtil::SgfStringToPoint;
using SgPropUtil::EscapeSpecialCharacters;
using SgUtil::InRange;

//----------------------------------------------------------------------------

SgPropPointFmt SgPropUtil::GetPointFmt(int gameNumber) {
  switch (gameNumber) {
    case 2: // Othello
    case 11: // Hex
      return SG_PROPPOINTFMT_HEX;
    default:return SG_PROPPOINTFMT_GO;
  }
}

string SgPropUtil::PointToSgfString(GoMove p, int boardSize,
                                    SgPropPointFmt fmt, int fileFormat) {
  DBG_ASSERT(boardSize >= GO_MIN_SIZE && boardSize <= GO_MAX_SIZE);
  DBG_ASSERT(p != GO_NULLMOVE);
  ostringstream out;
  switch (fmt) {
    case SG_PROPPOINTFMT_GO: {
      if (p == GO_PASS) {
        if (fileFormat < 4)
          out << "tt";
        // Pass is empty string in FF[4]
      } else {
        int col = GoPointUtil::Col(p);
        int row = boardSize - GoPointUtil::Row(p) + 1;
        DBG_ASSERT(row > 0);
        out << static_cast<char>('a' + col - 1)
            << static_cast<char>('a' + row - 1);
      }
    }
      break;
    case SG_PROPPOINTFMT_HEX: {
      DBG_ASSERT(p != GO_PASS);
      int col = GoPointUtil::Col(p);
      int row = boardSize - GoPointUtil::Row(p) + 1;
      out << static_cast<char>('a' + col - 1) << row;
    }
      break;
    default:DBG_ASSERT(false);
  }
  return out.str();
}

string SgPropUtil::EscapeSpecialCharacters(const string &s, bool escapeColon) {
  ostringstream buffer;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (c == ']' || c == '\\' || (c == ':' && escapeColon))
      buffer << '\\' << c;
    else if (c == '\r' || c == '\t')
      buffer << ' ';
    else
      buffer << c;
  }
  return buffer.str();
}

GoMove SgPropUtil::SgfStringToPoint(const string &s, int boardSize,
                                    SgPropPointFmt fmt) {
  DBG_ASSERT(boardSize >= GO_MIN_SIZE && boardSize <= GO_MAX_SIZE);
  GoPoint p = GO_NULLMOVE;
  switch (fmt) {
    case SG_PROPPOINTFMT_GO: {
      if (s.size() == 2
          && 'a' <= s[0] && s[0] <= 's'
          && 'a' <= s[1] && s[1] <= 's') {
        int col = s[0] - 'a' + 1;
        int row = s[1] - 'a' + 1;
        p = GoPointUtil::Pt(col, boardSize - row + 1);
      } else if (s.empty() // new FF[4] definition
          || (s.size() == 2 && s[0] == 't' && s[1] == 't'))
        p = GO_PASS;
    }
      break;
    case SG_PROPPOINTFMT_HEX: {
      if (s.size() >= 2 && s.size() <= 3) {
        int col = s[0] - 'a' + 1;
        int row = s[1] - '1' + 1;
        if (s.size() == 3)
          row = row * 10 + (s[2] - '1' + 1);
        if (InRange(col, 1, boardSize) && InRange(row, 1, boardSize))
          p = GoPointUtil::Pt(col, boardSize - row + 1);
      }
    }
      break;
    default:DBG_ASSERT(false);
  }
  return p;
}


SgPropList::SgPropList()
    : m_list() {}

SgPropList::~SgPropList() {
  Clear();
}

void SgPropList::Clear() {
  for (SgVectorIteratorOf<SgProp> iter(m_list); iter; ++iter)
    delete *iter;
  m_list.Clear();
}

SgProp *SgPropList::Get(SgPropID id) const {
  for (SgVectorIteratorOf<SgProp> iter(m_list); iter; ++iter) {
    SgProp *prop = *iter;
    if (prop->MatchesID(id))
      return prop;
  }
  return 0;
}

void SgPropList::Add(const SgProp *prop) {
  DBG_ASSERT(prop);

  // First remove any property of that type, then add new property.
  // Enforce constraint that there can be only one move annotation per node.
  // Can have multiple unknown properties per node.
  if (prop->ID() != SG_PROP_UNKNOWN) {
    if (prop->Flag(SG_PROPCLASS_ANNO_MOVE))
      Remove(SG_PROP_MOVE_ANNO, prop);
    else if (prop->Flag(SG_PROPCLASS_ANNO_POS))
      Remove(SG_PROP_POS_ANNO, prop);
    else if (prop->Flag(SG_PROPCLASS_MOVE))
      Remove(SG_PROP_MOVE, prop);
    else
      Remove(prop->ID(), prop);
  }
  m_list.Include(prop);
}

void SgPropList::MoveToFront(SgPropID id) {
  SgProp *prop = Get(id);
  if (prop && m_list.Exclude(prop))
    m_list.PushFront(prop);
}

bool SgPropList::Remove(const SgProp *prop) {
  const bool remove = m_list.Exclude(const_cast<SgProp *>(prop));
  if (prop)
    delete prop;
  return remove;
}

void SgPropList::Remove(SgPropID id, const SgProp *protectProp) {
  SgVectorOf<SgProp> toBeDeleted;
  for (SgVectorIteratorOf<SgProp> iter(m_list); iter; ++iter) {
    SgProp *prop = *iter;
    if (prop != protectProp && prop->MatchesID(id)) {
      // Can't exclude while iterating over same list.
      toBeDeleted.PushBack(prop);
      delete prop;
    }
  }
  m_list.Exclude(toBeDeleted);
}

bool SgPropList::AppendMoveAnnotation(string *s) const {
  SgProp *moveAnnoProp = Get(SG_PROP_MOVE_ANNO);
  if (!moveAnnoProp)
    return false;
  SgPropInt *intProp = dynamic_cast<SgPropInt *>(moveAnnoProp);
  int value = intProp ? intProp->Value() : 1;
  SgPropID id = moveAnnoProp->ID();
  if (id == SG_PROP_GOOD_MOVE)
    *s += (value == 2) ? "!!" : "!";
  else if (id == SG_PROP_BAD_MOVE)
    *s += (value == 2) ? "??" : "?";
  else if (id == SG_PROP_INTERESTING)
    *s += "!?";
  else if (id == SG_PROP_DOUBTFUL)
    *s += "?!";
  return true;
}

SgProp *SgPropList::GetPropContainingText(const string &findText) const {
  for (SgVectorIteratorOf<SgProp> iter(m_list); iter; ++iter) {
    SgProp *prop = *iter;
    if (prop->ContainsText(findText))
      return prop;
  }
  return 0;
}


SgPropID SG_PROP_NONE = 0;
SgPropID SG_PROP_UNKNOWN = 0;
//--- moves
SgPropID SG_PROP_MOVE = 0;
SgPropID SG_PROP_MOVE_BLACK = 0;
SgPropID SG_PROP_MOVE_WHITE = 0;
//--- board edits
SgPropID SG_PROP_ADD_BLACK = 0;
SgPropID SG_PROP_ADD_WHITE = 0;
SgPropID SG_PROP_ADD_EMPTY = 0;
SgPropID SG_PROP_PLAYER = 0;
//--- value and territory
SgPropID SG_PROP_VALUE = 0;
SgPropID SG_PROP_TERR_BLACK = 0;
SgPropID SG_PROP_TERR_WHITE = 0;
//--- marks drawn on the board
SgPropID SG_PROP_MARKS = 0;
SgPropID SG_PROP_SELECT = 0;
SgPropID SG_PROP_MARKED = 0;
SgPropID SG_PROP_TRIANGLE = 0;
SgPropID SG_PROP_SQUARE = 0;
SgPropID SG_PROP_DIAMOND = 0;
SgPropID SG_PROP_CIRCLE = 0;
SgPropID SG_PROP_DIMMED = 0;
SgPropID SG_PROP_LABEL = 0;
//--- time control
SgPropID SG_PROP_TIMES = 0;
SgPropID SG_PROP_TIME_BLACK = 0;
SgPropID SG_PROP_TIME_WHITE = 0;
SgPropID SG_PROP_OT_NU_MOVES = 0;
SgPropID SG_PROP_OT_PERIOD = 0;
SgPropID SG_PROP_OT_BLACK = 0;
SgPropID SG_PROP_OT_WHITE = 0;
SgPropID SG_PROP_LOSE_TIME = 0;
SgPropID SG_PROP_OVERHEAD = 0;
//--- statistics
SgPropID SG_PROP_COUNT = 0;
SgPropID SG_PROP_TIME_USED = 0;
SgPropID SG_PROP_NUM_NODES = 0;
SgPropID SG_PROP_NUM_LEAFS = 0;
SgPropID SG_PROP_MAX_DEPTH = 0;
SgPropID SG_PROP_DEPTH = 0;
SgPropID SG_PROP_PART_DEPTH = 0;
SgPropID SG_PROP_EVAL = 0;
SgPropID SG_PROP_EXPECTED = 0;
//--- root props
SgPropID SG_PROP_FORMAT = 0;
SgPropID SG_PROP_SIZE = 0;
SgPropID SG_PROP_GAME = 0;
SgPropID SG_PROP_SPEC_BLACK = 0;
SgPropID SG_PROP_SPEC_WHITE = 0;
SgPropID SG_PROP_CHINESE = 0;
SgPropID SG_PROP_APPLIC = 0;
//--- annotations
SgPropID SG_PROP_ANNOTATE = 0;
SgPropID SG_PROP_COMMENT = 0;
SgPropID SG_PROP_NAME = 0;
SgPropID SG_PROP_CHECK = 0;
SgPropID SG_PROP_SIGMA = 0;
SgPropID SG_PROP_HOTSPOT = 0;
SgPropID SG_PROP_FIGURE = 0;
//--- position annotations
SgPropID SG_PROP_POS_ANNO = 0;
SgPropID SG_PROP_GOOD_BLACK = 0;
SgPropID SG_PROP_GOOD_WHITE = 0;
SgPropID SG_PROP_EVEN_POS = 0;
SgPropID SG_PROP_UNCLEAR = 0;
//--- move annotations
SgPropID SG_PROP_MOVE_ANNO = 0;
SgPropID SG_PROP_GOOD_MOVE = 0;
SgPropID SG_PROP_BAD_MOVE = 0;
SgPropID SG_PROP_INTERESTING = 0;
SgPropID SG_PROP_DOUBTFUL = 0;
//--- game info
SgPropID SG_PROP_INFO = 0;
SgPropID SG_PROP_GAME_NAME = 0;
SgPropID SG_PROP_GAME_COMMENT = 0;
SgPropID SG_PROP_EVENT = 0;
SgPropID SG_PROP_ROUND = 0;
SgPropID SG_PROP_DATE = 0;
SgPropID SG_PROP_PLACE = 0;
SgPropID SG_PROP_PLAYER_BLACK = 0;
SgPropID SG_PROP_PLAYER_WHITE = 0;
SgPropID SG_PROP_RESULT = 0;
SgPropID SG_PROP_USER = 0;
SgPropID SG_PROP_TIME = 0;
SgPropID SG_PROP_SOURCE = 0;
SgPropID SG_PROP_COPYRIGHT = 0;
SgPropID SG_PROP_ANALYSIS = 0;
SgPropID SG_PROP_RANK_BLACK = 0;
SgPropID SG_PROP_RANK_WHITE = 0;
SgPropID SG_PROP_TEAM_BLACK = 0;
SgPropID SG_PROP_TEAM_WHITE = 0;
SgPropID SG_PROP_OPENING = 0;
SgPropID SG_PROP_RULES = 0;
SgPropID SG_PROP_HANDICAP = 0;
SgPropID SG_PROP_KOMI = 0;
//--- abstract properties
SgPropID SG_PROP_FIND_MOVE = 0;
SgPropID SG_PROP_FIND_TEXT = 0;
SgPropID SG_PROP_BRANCH = 0;
SgPropID SG_PROP_TERMINAL = 0;
//--- Smart Go specific properties
SgPropID SG_PROP_MOTIVE = 0;
SgPropID SG_PROP_SEQUENCE = 0;
SgPropID SG_PROP_NOT_EMPTY = 0;
SgPropID SG_PROP_NOT_BLACK = 0;
SgPropID SG_PROP_NOT_WHITE = 0;

//----------------------------------------------------------------------------

bool SgProp::s_initialized = false;
int SgProp::s_numPropClasses = 0;
SgPropFlags SgProp::s_flags[SG_MAX_PROPCLASS];
string SgProp::s_label[SG_MAX_PROPCLASS];
SgProp *SgProp::s_prop[SG_MAX_PROPCLASS];

SgProp::~SgProp() {}

void SgProp::ChangeToOpponent() {
  m_id = OpponentProp(m_id);
}

bool SgProp::ContainsText(const std::string &findText) {
  SuppressUnused(findText);
  return false;
}

SgPropFlags SgProp::Flags() const {
  return s_flags[m_id];
}

bool SgProp::Initialized() {
  return s_initialized;
}

string SgProp::Label() const {
  return s_label[m_id];
}

SgPropID SgProp::Register(SgProp *prop, const char *label, SgPropFlags flags) {
  ++s_numPropClasses;
  DBG_ASSERT(s_numPropClasses < SG_MAX_PROPCLASS);
  if (s_numPropClasses < SG_MAX_PROPCLASS) {
    s_flags[s_numPropClasses] = flags;
    s_label[s_numPropClasses] = label;
    s_prop[s_numPropClasses] = prop;
    if (prop) {
      DBG_ASSERT(prop->m_id == 0); // can't know the ID yet
    }
    // Black and white properties must be created in pairs, black first.
    if (flags & SG_PROPCLASS_WHITE)
      DBG_ASSERT(s_flags[s_numPropClasses - 1] & SG_PROPCLASS_BLACK);

    return s_numPropClasses;
  } else
    return 0;
}

SgProp *SgProp::CreateProperty(SgPropID id) {
  DBG_ASSERT(id <= s_numPropClasses);
  DBG_ASSERT(s_prop[id]);

  // Create a property of the right class, and set its ID (because
  // prototype that's duplicated contains a zero ID).
  SgProp *prop = s_prop[id]->Duplicate();
  prop->m_id = id;
  return prop;
}

SgPropID SgProp::GetIDOfLabel(const string &label) {
  for (int i = 1; i <= s_numPropClasses; ++i)
    if (s_label[i] == label)
      return i;
  return SG_PROP_NONE;
}

SgPropID SgProp::OpponentProp(SgPropID id) {
  // Relies on the fact that these properties are always created in pairs,
  // with black created before white.
  // @todo ---> Flags cannot really be overridden
  SgPropFlags flags = s_flags[id];

  if (flags & SG_PROPCLASS_BLACK) {
    ++id;
    DBG_ASSERT(s_flags[id] & SG_PROPCLASS_WHITE);
  } else if (flags & SG_PROPCLASS_WHITE) {
    --id;
    DBG_ASSERT(s_flags[id] & SG_PROPCLASS_BLACK);
  }
  return id;
}

SgPropID SgProp::PlayerProp(SgPropID id, SgBlackWhite player) {
  // @todo ---> Flags cannot really be overridden
  SgPropFlags flags = s_flags[id];
  DBG_ASSERT(flags & (SG_PROPCLASS_BLACK | SG_PROPCLASS_WHITE));
  int mask = (player == SG_WHITE ? SG_PROPCLASS_WHITE : SG_PROPCLASS_BLACK);
  if (flags & mask)
    return id;
  else
    return OpponentProp(id);
}

SgBlackWhite SgProp::Player() const {
  DBG_ASSERT(Flag(SG_PROPCLASS_BLACK | SG_PROPCLASS_WHITE));
  if (Flags() & SG_PROPCLASS_BLACK)
    return SG_BLACK;
  else if (Flags() & SG_PROPCLASS_WHITE)
    return SG_WHITE;
  DBG_ASSERT(false);
  return -1;
}

bool SgProp::IsPlayer(SgBlackWhite player) const {
  DBG_ASSERT_BW(player);
  return (Player() == player);
}

bool SgProp::MatchesID(SgPropID id) const {
  // Matches if ID matches exactly.
  if (id == ID())
    return true;

  // Matches if looking for abstract property and matches that category.
  if (s_flags[id] & SG_PROPCLASS_ABSTRACT) {
    SgPropFlags fCategories = s_flags[id] & (~SG_PROPCLASS_ABSTRACT);
    if ((Flags() & fCategories) == fCategories)
      return true;
  }

  return false;
}

SgPropID SgProp::ConvertFindTextToPropID(const string &findText) {
  size_t length = findText.size();
  if ((3 <= length && findText[1] == ' ' && findText[2] == '<')
      || (2 <= length && findText[1] == '<')
      || 1 == length
      ) {
    switch (findText[0]) {
      case 'A':return SG_PROP_ANNOTATE;
      case 'B':return SG_PROP_MOVE_BLACK;
      case 'C':return SG_PROP_COMMENT;
      case 'H':return SG_PROP_HOTSPOT;
      case 'I':return SG_PROP_INFO;
      case 'K':return SG_PROP_CHECK;
      case 'M':return SG_PROP_MARKS;
      case 'N':return SG_PROP_NAME;
      case 'S':return SG_PROP_SIGMA;
      case 'T':return SG_PROP_TRIANGLE;
      case 'W':return SG_PROP_MOVE_WHITE;
      case '!':return SG_PROP_GOOD_MOVE;
      case '?':return SG_PROP_BAD_MOVE;
      case '.':return SG_PROP_TERMINAL;
      case ':':return SG_PROP_BRANCH;
      default:break;
    }
  }
  return SG_PROP_NONE;
}

void SgProp::Init() {
  // Attributes of SG_PROP_NONE.
  s_flags[0] = 0;
  s_label[0] = "";
  s_prop[0] = 0;

  // Create prototype properties, one for each property class.
  SgProp *unknownProp = new SgPropUnknown(0);
  SgProp *simpleProp = new SgPropSimple(0);
  SgProp *intProp = new SgPropInt(0);
  SgProp *realProp = new SgPropReal(0);
  SgProp *multipleProp = new SgPropMultiple(0);
  SgProp *valueProp = new SgPropValue(0);
  SgProp *timeProp = new SgPropTime(0);
  SgProp *mSecProp = new SgPropMSec(0);
  SgProp *moveProp = new SgPropMove(0);
  SgProp *listProp = new SgPropPointList(0);
  SgProp *textProp = new SgPropText(0);
  SgProp *textListProp = new SgPropTextList(0);
  SgProp *playerProp = new SgPropPlayer(0);
  SgProp *addStoneProp = new SgPropAddStone(0);

  // Register abstract property classes so they get cleaned up on fini.
  SG_PROP_NONE = 0;
  SG_PROP_UNKNOWN = Register(unknownProp, "");
  Register(simpleProp, "");
  Register(intProp, "");
  Register(realProp, "");
  Register(multipleProp, "");
  Register(valueProp, "");
  Register(timeProp, "");
  Register(mSecProp, "");
  Register(moveProp, "");
  Register(listProp, "");
  Register(textProp, "");
  Register(textListProp, "");
  Register(playerProp, "");
  Register(addStoneProp, "");

  // Create the standard properties.

  //--- moves and board edits
  SG_PROP_MOVE = Register(0, "", SG_PROPCLASS_MOVE + SG_PROPCLASS_ABSTRACT);
  SG_PROP_PLAYER = Register(playerProp, "PL");
  SG_PROP_ADD_BLACK = Register(addStoneProp, "AB",
                               SG_PROPCLASS_BLACK + SG_PROPCLASS_NEWLINE);
  SG_PROP_ADD_WHITE = Register(addStoneProp, "AW",
                               SG_PROPCLASS_WHITE + SG_PROPCLASS_NEWLINE);
  SG_PROP_ADD_EMPTY = Register(addStoneProp, "AE", SG_PROPCLASS_NEWLINE);

  //--- value and territory
  SG_PROP_VALUE = Register(valueProp, "V");
  SG_PROP_TERR_BLACK = Register(listProp, "TB",
                                SG_PROPCLASS_BLACK + SG_PROPCLASS_NEWLINE);
  SG_PROP_TERR_WHITE = Register(listProp, "TW",
                                SG_PROPCLASS_WHITE + SG_PROPCLASS_NEWLINE);

  //--- marks drawn on the board
  SG_PROP_MARKS = Register(0, "", SG_PROPCLASS_MARK + SG_PROPCLASS_ABSTRACT);
  SG_PROP_SELECT = Register(listProp, "SL", SG_PROPCLASS_MARK);
  SG_PROP_MARKED = Register(listProp, "MA", SG_PROPCLASS_MARK);
  SG_PROP_TRIANGLE = Register(listProp, "TR", SG_PROPCLASS_MARK);
  SG_PROP_SQUARE = Register(listProp, "SQ", SG_PROPCLASS_MARK);
  SG_PROP_DIAMOND = Register(listProp, "RG", SG_PROPCLASS_MARK);
  SG_PROP_CIRCLE = Register(listProp, "CR", SG_PROPCLASS_MARK);
  SG_PROP_DIMMED = Register(listProp, "DD", SG_PROPCLASS_MARK);
  SG_PROP_LABEL = Register(textListProp, "LB", SG_PROPCLASS_MARK);

  //--- time control
  SG_PROP_TIMES = Register(0, "", SG_PROPCLASS_TIME + SG_PROPCLASS_ABSTRACT);
  SG_PROP_TIME_BLACK = Register(timeProp, "BL",
                                SG_PROPCLASS_TIME + SG_PROPCLASS_BLACK);
  SG_PROP_TIME_WHITE = Register(timeProp, "WL",
                                SG_PROPCLASS_TIME + SG_PROPCLASS_WHITE);
  SG_PROP_OT_BLACK = Register(intProp, "OB",
                              SG_PROPCLASS_TIME + SG_PROPCLASS_BLACK
                                  + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_OT_WHITE = Register(intProp, "OW",
                              SG_PROPCLASS_TIME + SG_PROPCLASS_WHITE
                                  + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_OT_NU_MOVES = Register(intProp, "OM",
                                 SG_PROPCLASS_TIME + SG_PROPCLASS_ROOT
                                     + SG_PROPCLASS_CUSTOM
                                     + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_OT_PERIOD = Register(timeProp, "OP",
                               SG_PROPCLASS_TIME + SG_PROPCLASS_ROOT
                                   + SG_PROPCLASS_CUSTOM + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_OVERHEAD = Register(timeProp, "OV",
                              SG_PROPCLASS_TIME + SG_PROPCLASS_ROOT
                                  + SG_PROPCLASS_CUSTOM + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_LOSE_TIME = Register(simpleProp, "LT",
                               SG_PROPCLASS_TIME + SG_PROPCLASS_ROOT
                                   + SG_PROPCLASS_CUSTOM + SG_PROPCLASS_NOTCLEAN);

  //--- statistics
  // @todo is official property?
  SG_PROP_COUNT = Register(0, "CN",
                           SG_PROPCLASS_STAT + SG_PROPCLASS_ABSTRACT);
  SG_PROP_TIME_USED = Register(mSecProp, "TU",
                               SG_PROPCLASS_STAT + SG_PROPCLASS_CUSTOM
                                   + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_NUM_NODES = Register(intProp, "NN",
                               SG_PROPCLASS_STAT + SG_PROPCLASS_CUSTOM
                                   + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_NUM_LEAFS = Register(intProp, "NL",
                               SG_PROPCLASS_STAT + SG_PROPCLASS_CUSTOM
                                   + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_MAX_DEPTH = Register(intProp, "MD",
                               SG_PROPCLASS_STAT + SG_PROPCLASS_CUSTOM
                                   + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_DEPTH = Register(intProp, "DE",
                           SG_PROPCLASS_STAT + SG_PROPCLASS_CUSTOM
                               + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_PART_DEPTH = Register(intProp, "PD",
                                SG_PROPCLASS_STAT + SG_PROPCLASS_CUSTOM
                                    + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_EVAL = Register(valueProp, "EL",
                          SG_PROPCLASS_STAT + SG_PROPCLASS_CUSTOM
                              + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_EXPECTED = Register(moveProp, "EX",
                              SG_PROPCLASS_STAT + SG_PROPCLASS_CUSTOM
                                  + SG_PROPCLASS_NOTCLEAN);

  //--- root props
  SG_PROP_FORMAT = Register(intProp, "FF", SG_PROPCLASS_ROOT);
  SG_PROP_SIZE = Register(intProp, "SZ", SG_PROPCLASS_ROOT);
  SG_PROP_GAME = Register(intProp, "GM", SG_PROPCLASS_ROOT);
  // @todo not root props?
  SG_PROP_SPEC_BLACK = Register(intProp, "BS", SG_PROPCLASS_CUSTOM);
  SG_PROP_SPEC_WHITE = Register(intProp, "WS", SG_PROPCLASS_CUSTOM);
  SG_PROP_CHINESE = Register(intProp, "CI",
                             SG_PROPCLASS_ROOT + SG_PROPCLASS_CUSTOM);
  SG_PROP_APPLIC = Register(textProp, "AP",
                            SG_PROPCLASS_ROOT + SG_PROPCLASS_NOT_FF3);

  //--- annotations
  SG_PROP_ANNOTATE = Register(0, "",
                              SG_PROPCLASS_ANNO + SG_PROPCLASS_ABSTRACT);
  SG_PROP_COMMENT = Register(textProp, "C",
                             SG_PROPCLASS_ANNO + SG_PROPCLASS_NEWLINE);
  SG_PROP_NAME = Register(textProp, "N",
                          SG_PROPCLASS_ANNO + SG_PROPCLASS_NEWLINE);
  SG_PROP_CHECK = Register(multipleProp, "CH",
                           SG_PROPCLASS_ANNO + SG_PROPCLASS_CUSTOM);
  SG_PROP_SIGMA = Register(multipleProp, "SI",
                           SG_PROPCLASS_ANNO + SG_PROPCLASS_CUSTOM);
  SG_PROP_HOTSPOT = Register(multipleProp, "HO",
                             SG_PROPCLASS_ANNO + SG_PROPCLASS_CUSTOM);
  SG_PROP_FIGURE = Register(simpleProp, "FG",
                            SG_PROPCLASS_ANNO + SG_PROPCLASS_NEWLINE);

  //--- position annotations
  SG_PROP_POS_ANNO = Register(0, "",
                              SG_PROPCLASS_ANNO + SG_PROPCLASS_ANNO_POS
                                  + SG_PROPCLASS_ABSTRACT);
  SG_PROP_GOOD_BLACK = Register(multipleProp, "GB",
                                SG_PROPCLASS_ANNO + SG_PROPCLASS_ANNO_POS
                                    + SG_PROPCLASS_BLACK);
  SG_PROP_GOOD_WHITE = Register(multipleProp, "GW",
                                SG_PROPCLASS_ANNO + SG_PROPCLASS_ANNO_POS
                                    + SG_PROPCLASS_WHITE);
  SG_PROP_EVEN_POS = Register(multipleProp, "DM",
                              SG_PROPCLASS_ANNO + SG_PROPCLASS_ANNO_POS);
  SG_PROP_UNCLEAR = Register(multipleProp, "UC",
                             SG_PROPCLASS_ANNO + SG_PROPCLASS_ANNO_POS);

  //--- move annotations
  SG_PROP_MOVE_ANNO = Register(0, "",
                               SG_PROPCLASS_ANNO + SG_PROPCLASS_ANNO_MOVE
                                   + SG_PROPCLASS_ABSTRACT);
  SG_PROP_GOOD_MOVE = Register(multipleProp, "TE",
                               SG_PROPCLASS_ANNO + SG_PROPCLASS_ANNO_MOVE);
  SG_PROP_BAD_MOVE = Register(multipleProp, "BM",
                              SG_PROPCLASS_ANNO + SG_PROPCLASS_ANNO_MOVE);
  SG_PROP_INTERESTING = Register(simpleProp, "IT",
                                 SG_PROPCLASS_ANNO + SG_PROPCLASS_ANNO_MOVE);
  SG_PROP_DOUBTFUL = Register(simpleProp, "DO",
                              SG_PROPCLASS_ANNO + SG_PROPCLASS_ANNO_MOVE);

  //--- game info
  SG_PROP_INFO = Register(0, "", SG_PROPCLASS_INFO + SG_PROPCLASS_ABSTRACT);
  SG_PROP_GAME_NAME = Register(textProp, "GN",
                               SG_PROPCLASS_INFO + SG_PROPCLASS_NEWLINE);
  SG_PROP_GAME_COMMENT = Register(textProp, "GC",
                                  SG_PROPCLASS_INFO + SG_PROPCLASS_NEWLINE);
  SG_PROP_EVENT = Register(textProp, "EV",
                           SG_PROPCLASS_INFO + SG_PROPCLASS_NEWLINE);
  SG_PROP_ROUND = Register(textProp, "RO", SG_PROPCLASS_INFO);
  SG_PROP_DATE = Register(textProp, "DT",
                          SG_PROPCLASS_INFO + SG_PROPCLASS_NEWLINE);
  SG_PROP_PLACE = Register(textProp, "PC",
                           SG_PROPCLASS_INFO + SG_PROPCLASS_NEWLINE);
  SG_PROP_PLAYER_BLACK = Register(textProp, "PB",
                                  SG_PROPCLASS_INFO + SG_PROPCLASS_BLACK
                                      + SG_PROPCLASS_NEWLINE);
  SG_PROP_PLAYER_WHITE = Register(textProp, "PW",
                                  SG_PROPCLASS_INFO + SG_PROPCLASS_WHITE
                                      + SG_PROPCLASS_NEWLINE);
  SG_PROP_RESULT = Register(textProp, "RE",
                            SG_PROPCLASS_INFO + SG_PROPCLASS_NEWLINE);
  SG_PROP_USER = Register(textProp, "US",
                          SG_PROPCLASS_INFO + SG_PROPCLASS_NEWLINE);
  SG_PROP_TIME = Register(textProp, "TM", SG_PROPCLASS_INFO);
  SG_PROP_SOURCE = Register(textProp, "SO",
                            SG_PROPCLASS_INFO + SG_PROPCLASS_NEWLINE);
  SG_PROP_COPYRIGHT = Register(textProp, "CP", SG_PROPCLASS_INFO);
  SG_PROP_ANALYSIS = Register(textProp, "AN", SG_PROPCLASS_INFO);
  SG_PROP_RANK_BLACK = Register(textProp, "BR",
                                SG_PROPCLASS_INFO + SG_PROPCLASS_BLACK);
  SG_PROP_RANK_WHITE = Register(textProp, "WR",
                                SG_PROPCLASS_INFO + SG_PROPCLASS_WHITE);
  SG_PROP_TEAM_BLACK = Register(textProp, "BT",
                                SG_PROPCLASS_INFO + SG_PROPCLASS_BLACK);
  SG_PROP_TEAM_WHITE = Register(textProp, "WT", SG_PROPCLASS_INFO + SG_PROPCLASS_WHITE);
  SG_PROP_OPENING = Register(textProp, "ON",
                             SG_PROPCLASS_INFO + SG_PROPCLASS_NEWLINE);
  SG_PROP_RULES = Register(textProp, "RU",
                           SG_PROPCLASS_INFO + SG_PROPCLASS_NEWLINE);
  SG_PROP_HANDICAP = Register(intProp, "HA", SG_PROPCLASS_INFO);
  SG_PROP_KOMI = Register(realProp, "KM", SG_PROPCLASS_INFO);

  //--- abstract properties
  SG_PROP_FIND_MOVE = Register(0, "", SG_PROPCLASS_ABSTRACT);
  SG_PROP_FIND_TEXT = Register(0, "", SG_PROPCLASS_ABSTRACT);
  SG_PROP_BRANCH = Register(0, "", SG_PROPCLASS_ABSTRACT);
  SG_PROP_TERMINAL = Register(0, "", SG_PROPCLASS_ABSTRACT);

  //--- Smart Go specific properties
  SG_PROP_MOTIVE = Register(textListProp, "MM",
                            SG_PROPCLASS_STAT + SG_PROPCLASS_CUSTOM
                                + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_SEQUENCE = Register(listProp, "MS",
                              SG_PROPCLASS_STAT + SG_PROPCLASS_CUSTOM
                                  + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_NOT_EMPTY = Register(listProp, "NE",
                               SG_PROPCLASS_MARK + SG_PROPCLASS_CUSTOM
                                   + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_NOT_BLACK = Register(listProp, "NB",
                               SG_PROPCLASS_MARK + SG_PROPCLASS_CUSTOM
                                   + SG_PROPCLASS_NOTCLEAN);
  SG_PROP_NOT_WHITE = Register(listProp, "NW",
                               SG_PROPCLASS_MARK + SG_PROPCLASS_CUSTOM
                                   + SG_PROPCLASS_NOTCLEAN);

  s_initialized = true;
}

void SgProp::Fini() {
  s_initialized = false;
}

//----------------------------------------------------------------------------

SgProp *SgPropUnknown::Duplicate() const {
  return new SgPropUnknown(m_id, m_label, m_values);
}

bool SgPropUnknown::ToString(std::vector<std::string> &values, int boardSize,
                             SgPropPointFmt fmt, int fileFormat) const {
  SuppressUnused(boardSize);
  SuppressUnused(fmt);
  SuppressUnused(fileFormat);
  values.clear();
  for (vector<string>::const_iterator it = m_values.begin();
       it != m_values.end(); ++it)
    values.push_back(EscapeSpecialCharacters(*it, false));
  return true;
}

bool SgPropUnknown::FromString(const std::vector<std::string> &values,
                               int boardSize, SgPropPointFmt fmt) {
  SuppressUnused(boardSize);
  SuppressUnused(fmt);
  m_values = values;
  return true;
}

//----------------------------------------------------------------------------

SgProp *SgPropSimple::Duplicate() const {
  return new SgPropSimple(m_id);
}

bool SgPropSimple::ToString(std::vector<std::string> &values, int boardSize,
                            SgPropPointFmt fmt, int fileFormat) const {
  SuppressUnused(boardSize);
  SuppressUnused(fmt);
  SuppressUnused(fileFormat);
  values.assign(1, "");
  return true;
}

bool SgPropSimple::FromString(const std::vector<std::string> &values,
                              int boardSize, SgPropPointFmt fmt) {
  SuppressUnused(values);
  SuppressUnused(boardSize);
  SuppressUnused(fmt);
  return true;
}

//----------------------------------------------------------------------------

SgProp *SgPropMultiple::Duplicate() const {
  return new SgPropMultiple(m_id, m_value);
}

//----------------------------------------------------------------------------

SgProp *SgPropInt::Duplicate() const {
  return new SgPropInt(m_id, m_value);
}

bool SgPropInt::ToString(std::vector<std::string> &values, int boardSize,
                         SgPropPointFmt fmt, int fileFormat) const {
  SuppressUnused(fileFormat);
  SuppressUnused(boardSize);
  SuppressUnused(fmt);
  ostringstream buffer;
  buffer << m_value;
  values.assign(1, buffer.str());
  return true;
}

bool SgPropInt::FromString(const std::vector<std::string> &values,
                           int boardSize, SgPropPointFmt fmt) {
  SuppressUnused(boardSize);
  SuppressUnused(fmt);
  if (values.empty()) {
    m_value = 0;
    return true;
  }
  istringstream in(values[0]);
  in >> m_value;
  return (!in.fail());
}

//----------------------------------------------------------------------------

SgProp *SgPropReal::Duplicate() const {
  return new SgPropReal(m_id, m_value, m_precision);
}

bool SgPropReal::ToString(std::vector<std::string> &values, int boardSize,
                          SgPropPointFmt fmt, int fileFormat) const {
  SuppressUnused(boardSize);
  SuppressUnused(fmt);
  SuppressUnused(fileFormat);
  ostringstream buffer;
  if (m_precision > 0)
    buffer.precision(m_precision);
  buffer << fixed << m_value;
  values.assign(1, buffer.str());
  return true;
}

bool SgPropReal::FromString(const std::vector<std::string> &values,
                            int boardSize, SgPropPointFmt fmt) {
  SuppressUnused(boardSize);
  SuppressUnused(fmt);
  if (values.empty()) {
    m_value = 0;
    return true;
  }
  istringstream in(values[0]);
  in >> m_value;
  return (!in.fail());
}

//----------------------------------------------------------------------------

void SgPropValue::ChangeToOpponent() {
  m_value = -m_value;
}

SgProp *SgPropValue::Duplicate() const {
  return new SgPropValue(m_id, m_value);
}

//----------------------------------------------------------------------------

SgPropTime::~SgPropTime() {}

SgProp *SgPropTime::Duplicate() const {
  return new SgPropTime(m_id, m_value, m_precision);
}

//----------------------------------------------------------------------------

SgPropMSec::~SgPropMSec() {}

SgProp *SgPropMSec::Duplicate() const {
  return new SgPropMSec(m_id, m_value);
}

//----------------------------------------------------------------------------

SgPropPointList::SgPropPointList(SgPropID id,
                                 const SgVector<GoPoint> &vector)
    : SgProp(id),
      m_list(vector) {}

SgPropPointList::~SgPropPointList() {}

SgProp *SgPropPointList::Duplicate() const {
  return new SgPropPointList(ID(), Value());
}

bool SgPropPointList::ToString(std::vector<std::string> &values,
                               int boardSize, SgPropPointFmt fmt,
                               int fileFormat) const {
  // Don't write out empty list properties.
  if (Value().IsEmpty())
    return false;
  values.clear();
  for (SgVectorIterator<GoPoint> it(Value()); it; ++it) {
    if (!SgIsSpecialMove(*it))
      values.push_back(PointToSgfString(*it, boardSize, fmt,
                                        fileFormat));
  }
  return true;
}

bool SgPropPointList::FromString(const std::vector<std::string> &values,
                                 int boardSize, SgPropPointFmt fmt) {
  for (auto it = values.begin();
       it != values.end(); ++it) {
    string s = *it;
    if (s.size() == 5 && s[2] == ':') {
      // Compressed point list.
      string s1 = s.substr(0, 2);
      string s2 = s.substr(3, 2);
      GoPoint p1 = SgfStringToPoint(s1, boardSize, fmt);
      GoPoint p2 = SgfStringToPoint(s2, boardSize, fmt);
      if (InBoardRange(p1) && InBoardRange(p2)) {
        GoRect rect(p1, p2);
        for (SgRectIterator iter(rect); iter; ++iter)
          Value().PushBack(*iter);
      } else {
        return false;
      }
    } else {
      // Single point.
      GoPoint p = SgfStringToPoint(s, boardSize, fmt);
      if (InBoardRange(p) || p == GO_PASS)
        // Pass needed for move sequence
        Value().PushBack(p);
      else
        return false;
    }
  }
  return true;
}

//----------------------------------------------------------------------------

SgProp *SgPropText::Duplicate() const {
  return new SgPropText(m_id, m_text);
}

bool SgPropText::ToString(std::vector<std::string> &values, int boardSize,
                          SgPropPointFmt fmt, int fileFormat) const {
  SuppressUnused(fileFormat);
  SuppressUnused(boardSize);
  SuppressUnused(fmt);
  values.assign(1, EscapeSpecialCharacters(m_text, false));
  return true;
}

bool SgPropText::FromString(const std::vector<std::string> &values,
                            int boardSize, SgPropPointFmt fmt) {
  SuppressUnused(boardSize);
  SuppressUnused(fmt);
  if (values.size() == 0)
    m_text = "";
  else
    m_text = values[0];
  return true;
}

bool SgPropText::ContainsText(const string &findText) {
  return (m_text.find(findText) != string::npos);
}

//----------------------------------------------------------------------------

SgPropTextList::SgPropTextList(SgPropID id, const SgVector<GoPoint> &points,
                               SgVectorOf<std::string> strings)
    : SgProp(id),
      m_points(points),
      m_strings() {
  for (SgVectorIteratorOf<string> it(strings); it; ++it) {
    m_strings.PushBack(new string(*(*it)));
  }
}

SgPropTextList::~SgPropTextList() {
  for (SgVectorIteratorOf<string> it(m_strings); it; ++it) {
    delete *it;
  }
}

SgProp *SgPropTextList::Duplicate() const {
  return new SgPropTextList(m_id, m_points, m_strings);
}

bool SgPropTextList::GetStringAtPoint(GoPoint p, string *s) const {
  int index = m_points.Index(p);
  if (index >= 0) {
    *s = *m_strings[index];
    return true;
  }
  return false;
}

void SgPropTextList::AddStringAtPoint(GoPoint p, const string &s) {
  ClearStringAtPoint(p);
  m_points.PushBack(p);
  m_strings.PushBack(new string(s));
}

void SgPropTextList::AppendToStringAtPoint(GoPoint p, const string &s) {
  string value;
  GetStringAtPoint(p, &value); // may be empty.
  value += s;
  AddStringAtPoint(p, value); // this clears out old string, if any.
}

void SgPropTextList::ClearStringAtPoint(GoPoint p) {
  int index = m_points.Index(p);
  if (index >= 0) {
    m_points.DeleteAt(index);
    delete m_strings[index];
    m_strings.DeleteAt(index);
  }
}

bool SgPropTextList::ToString(std::vector<std::string> &values,
                              int boardSize, SgPropPointFmt fmt,
                              int fileFormat) const {
  // Don't write out empty text list properties.
  if (m_points.IsEmpty())
    return false;
  values.clear();
  int index = 0;
  for (SgVectorIterator<GoPoint> it(m_points); it; ++it) {
    ostringstream buffer;
    buffer << PointToSgfString(*it, boardSize, fmt, fileFormat) << ':'
           << EscapeSpecialCharacters(*m_strings[index], true);
    values.push_back(buffer.str());
    ++index;
  }
  return true;
}

bool SgPropTextList::FromString(const std::vector<std::string> &values,
                                int boardSize, SgPropPointFmt fmt) {
  for (auto it = values.begin();
       it != values.end(); ++it) {
    string s = *it;
    if (s.size() >= 2) {
      string pointString = s.substr(0, 2);
      GoPoint p = SgfStringToPoint(pointString, boardSize, fmt);
      if (InBoardRange(p) && s.size() >= 3 && s[2] == ':') {
        string text = s.substr(3);
        AddStringAtPoint(p, text);
        return true;
      }
    }
  }
  return false;
}

bool SgPropTextList::ContainsText(const string &findText) {
  for (SgVectorIteratorOf<string> it(m_strings); it; ++it) {
    if ((*it)->find(findText) != string::npos)
      return true;
  }
  return false;
}

//----------------------------------------------------------------------------

SgProp *SgPropPlayer::Duplicate() const {
  return new SgPropPlayer(m_id, m_player);
}

bool SgPropPlayer::ToString(std::vector<std::string> &values, int boardSize,
                            SgPropPointFmt fmt, int fileFormat) const {
  SuppressUnused(fileFormat);
  SuppressUnused(boardSize);
  SuppressUnused(fmt);
  values.assign(1, m_player == SG_WHITE ? "W" : "B");
  return true;
}

bool SgPropPlayer::FromString(const std::vector<std::string> &values,
                              int boardSize, SgPropPointFmt fmt) {
  SuppressUnused(boardSize);
  SuppressUnused(fmt);
  if (values.size() == 0)
    return false;
  m_player = (values[0] == "W" || values[0] == "w") ? SG_WHITE : SG_BLACK;
  return true;
}

void SgPropPlayer::ChangeToOpponent() {
  m_player = SgOppBW(m_player);
}

//----------------------------------------------------------------------------

SgPropAddStone::~SgPropAddStone() {}

SgProp *SgPropAddStone::Duplicate() const {
  return new SgPropAddStone(m_id, Value());
}

//----------------------------------------------------------------------------

SgProp *SgPropMove::Duplicate() const {
  return new SgPropMove(m_id, m_move);
}

bool SgPropMove::ToString(std::vector<std::string> &values, int boardSize,
                          SgPropPointFmt fmt, int fileFormat) const {
  values.assign(1, PointToSgfString(m_move, boardSize, fmt, fileFormat));
  return true;
}

bool SgPropMove::FromString(const std::vector<std::string> &values,
                            int boardSize, SgPropPointFmt fmt) {
  if (values.size() == 0)
    return false;
  m_move = SgfStringToPoint(values[0], boardSize, fmt);
  return m_move != GO_NULLMOVE;
}
