//----------------------------------------------------------------------------

#include "platform/SgSystem.h"

#include <boost/test/auto_unit_test.hpp>
#include "SgEvaluatedMoves.h"

#include "board/GoPoint.h"
#include "lib/SgSortedArray.h"

using GoPointUtil::Pt;

namespace {

BOOST_AUTO_TEST_CASE(SgSortedArrayTest) {
  SgSortedArray<GoPoint, int, 5> a;
  SgVector<GoPoint> vec;
  a.GetElements(&vec);
  BOOST_CHECK(vec.IsEmpty());

  a.AddItem(Pt(3, 4), 1000);
  a.AddItem(Pt(3, 5), 100);
  a.AddItem(Pt(3, 6), 10);
  a.AddItem(Pt(3, 7), 1);
  a.AddItem(Pt(3, 8), 10000);

  a.SortMaximize();
  a.GetElements(&vec);
  BOOST_CHECK(vec.IsLength(5));
  BOOST_CHECK_EQUAL(vec[0], Pt(3, 8));
  BOOST_CHECK_EQUAL(vec[1], Pt(3, 4));
  BOOST_CHECK_EQUAL(vec[2], Pt(3, 5));
  BOOST_CHECK_EQUAL(vec[3], Pt(3, 6));
  BOOST_CHECK_EQUAL(vec[4], Pt(3, 7));
  a.SortMinimize();
  a.GetElements(&vec);
  BOOST_CHECK(vec.IsLength(5));
  BOOST_CHECK_EQUAL(vec[0], Pt(3, 7));
  BOOST_CHECK_EQUAL(vec[1], Pt(3, 6));
  BOOST_CHECK_EQUAL(vec[2], Pt(3, 5));
  BOOST_CHECK_EQUAL(vec[3], Pt(3, 4));
  BOOST_CHECK_EQUAL(vec[4], Pt(3, 8));
}

}

