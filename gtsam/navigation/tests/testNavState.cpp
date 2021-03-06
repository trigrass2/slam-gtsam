/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file    testNavState.cpp
 * @brief   Unit tests for NavState
 * @author  Frank Dellaert
 * @date    July 2015
 */

#include <gtsam/navigation/NavState.h>
#include <gtsam/base/TestableAssertions.h>
#include <gtsam/base/numericalDerivative.h>
#include <CppUnitLite/TestHarness.h>

using namespace std;
using namespace gtsam;

static const Rot3 kAttitude = Rot3::RzRyRx(0.1, 0.2, 0.3);
static const Point3 kPosition(1.0, 2.0, 3.0);
static const Pose3 kPose(kAttitude, kPosition);
static const Velocity3 kVelocity(0.4, 0.5, 0.6);
static const NavState kIdentity;
static const NavState kState1(kAttitude, kPosition, kVelocity);
static const Vector3 kOmegaCoriolis(0.02, 0.03, 0.04);
static const Vector3 kGravity(0, 0, 9.81);
static const Vector9 kZeroXi = Vector9::Zero();

/* ************************************************************************* */
TEST(NavState, Constructor) {
  boost::function<NavState(const Pose3&, const Vector3&)> construct =
      boost::bind(&NavState::FromPoseVelocity, _1, _2, boost::none,
          boost::none);
  Matrix aH1, aH2;
  EXPECT(
      assert_equal(kState1,
          NavState::FromPoseVelocity(kPose, kVelocity, aH1, aH2)));
  EXPECT(assert_equal(numericalDerivative21(construct, kPose, kVelocity), aH1));
  EXPECT(assert_equal(numericalDerivative22(construct, kPose, kVelocity), aH2));
}

/* ************************************************************************* */
TEST( NavState, Attitude) {
  Matrix39 aH, eH;
  Rot3 actual = kState1.attitude(aH);
  EXPECT(assert_equal(actual, kAttitude));
  eH = numericalDerivative11<Rot3, NavState>(
      boost::bind(&NavState::attitude, _1, boost::none), kState1);
  EXPECT(assert_equal((Matrix )eH, aH));
}

/* ************************************************************************* */
TEST( NavState, Position) {
  Matrix39 aH, eH;
  Point3 actual = kState1.position(aH);
  EXPECT(assert_equal(actual, kPosition));
  eH = numericalDerivative11<Point3, NavState>(
      boost::bind(&NavState::position, _1, boost::none), kState1);
  EXPECT(assert_equal((Matrix )eH, aH));
}

/* ************************************************************************* */
TEST( NavState, Velocity) {
  Matrix39 aH, eH;
  Velocity3 actual = kState1.velocity(aH);
  EXPECT(assert_equal(actual, kVelocity));
  eH = numericalDerivative11<Velocity3, NavState>(
      boost::bind(&NavState::velocity, _1, boost::none), kState1);
  EXPECT(assert_equal((Matrix )eH, aH));
}

/* ************************************************************************* */
TEST( NavState, BodyVelocity) {
  Matrix39 aH, eH;
  Velocity3 actual = kState1.bodyVelocity(aH);
  EXPECT(assert_equal(actual, kAttitude.unrotate(kVelocity)));
  eH = numericalDerivative11<Velocity3, NavState>(
      boost::bind(&NavState::bodyVelocity, _1, boost::none), kState1);
  EXPECT(assert_equal((Matrix )eH, aH));
}

/* ************************************************************************* */
TEST( NavState, MatrixGroup ) {
  // check roundtrip conversion to 7*7 matrix representation
  Matrix7 T = kState1.matrix();
  EXPECT(assert_equal(kState1, NavState(T)));

  // check group product agrees with matrix product
  NavState state2 = kState1 * kState1;
  Matrix T2 = T * T;
  EXPECT(assert_equal(state2, NavState(T2)));
  EXPECT(assert_equal(T2, state2.matrix()));
}

/* ************************************************************************* */
TEST( NavState, Manifold ) {
  // Check zero xi
  EXPECT(assert_equal(kIdentity, kIdentity.retract(kZeroXi)));
  EXPECT(assert_equal(kZeroXi, kIdentity.localCoordinates(kIdentity)));
  EXPECT(assert_equal(kState1, kState1.retract(kZeroXi)));
  EXPECT(assert_equal(kZeroXi, kState1.localCoordinates(kState1)));

  // Check definition of retract as operating on components separately
  Vector9 xi;
  xi << 0.1, 0.1, 0.1, 0.2, 0.3, 0.4, -0.1, -0.2, -0.3;
  Rot3 drot = Rot3::Expmap(xi.head<3>());
  Point3 dt = Point3(xi.segment<3>(3));
  Velocity3 dvel = Velocity3(-0.1, -0.2, -0.3);
  NavState state2 = kState1 * NavState(drot, dt, dvel);
  EXPECT(assert_equal(state2, kState1.retract(xi)));
  EXPECT(assert_equal(xi, kState1.localCoordinates(state2)));

  // roundtrip from state2 to state3 and back
  NavState state3 = state2.retract(xi);
  EXPECT(assert_equal(xi, state2.localCoordinates(state3)));

  // Check derivatives for ChartAtOrigin::Retract
  Matrix9 aH;
  //  For zero xi
  boost::function<NavState(const Vector9&)> Retract = boost::bind(
      NavState::ChartAtOrigin::Retract, _1, boost::none);
  NavState::ChartAtOrigin::Retract(kZeroXi, aH);
  EXPECT(assert_equal(numericalDerivative11(Retract, kZeroXi), aH));
  //  For non-zero xi
  NavState::ChartAtOrigin::Retract(xi, aH);
  EXPECT(assert_equal(numericalDerivative11(Retract, xi), aH));

  // Check derivatives for ChartAtOrigin::Local
  //  For zero xi
  boost::function<Vector9(const NavState&)> Local = boost::bind(
      NavState::ChartAtOrigin::Local, _1, boost::none);
  NavState::ChartAtOrigin::Local(kIdentity, aH);
  EXPECT(assert_equal(numericalDerivative11(Local, kIdentity), aH));
  //  For non-zero xi
  NavState::ChartAtOrigin::Local(kState1, aH);
  EXPECT(assert_equal(numericalDerivative11(Local, kState1), aH));

  // Check retract derivatives
  Matrix9 aH1, aH2;
  kState1.retract(xi, aH1, aH2);
  boost::function<NavState(const NavState&, const Vector9&)> retract =
      boost::bind(&NavState::retract, _1, _2, boost::none, boost::none);
  EXPECT(assert_equal(numericalDerivative21(retract, kState1, xi), aH1));
  EXPECT(assert_equal(numericalDerivative22(retract, kState1, xi), aH2));

  // Check localCoordinates derivatives
  boost::function<Vector9(const NavState&, const NavState&)> local =
      boost::bind(&NavState::localCoordinates, _1, _2, boost::none,
          boost::none);
  // from state1 to state2
  kState1.localCoordinates(state2, aH1, aH2);
  EXPECT(assert_equal(numericalDerivative21(local, kState1, state2), aH1));
  EXPECT(assert_equal(numericalDerivative22(local, kState1, state2), aH2));
  // from identity to state2
  kIdentity.localCoordinates(state2, aH1, aH2);
  EXPECT(assert_equal(numericalDerivative21(local, kIdentity, state2), aH1));
  EXPECT(assert_equal(numericalDerivative22(local, kIdentity, state2), aH2));
  // from state2 to identity
  state2.localCoordinates(kIdentity, aH1, aH2);
  EXPECT(assert_equal(numericalDerivative21(local, state2, kIdentity), aH1));
  EXPECT(assert_equal(numericalDerivative22(local, state2, kIdentity), aH2));
}

/* ************************************************************************* */
TEST( NavState, Lie ) {
  // Check zero xi
  EXPECT(assert_equal(kIdentity, kIdentity.expmap(kZeroXi)));
  EXPECT(assert_equal(kZeroXi, kIdentity.logmap(kIdentity)));
  EXPECT(assert_equal(kState1, kState1.expmap(kZeroXi)));
  EXPECT(assert_equal(kZeroXi, kState1.logmap(kState1)));

  // Expmap/Logmap roundtrip
  Vector xi(9);
  xi << 0.1, 0.1, 0.1, 0.2, 0.3, 0.4, -0.1, -0.2, -0.3;
  NavState state2 = NavState::Expmap(xi);
  EXPECT(assert_equal(xi, NavState::Logmap(state2)));

  // roundtrip from state2 to state3 and back
  NavState state3 = state2.expmap(xi);
  EXPECT(assert_equal(xi, state2.logmap(state3)));

  // For the expmap/logmap (not necessarily expmap/local) -xi goes other way
  EXPECT(assert_equal(state2, state3.expmap(-xi)));
  EXPECT(assert_equal(xi, -state3.logmap(state2)));
}

/* ************************************************************************* */
TEST(NavState, Update) {
  Vector3 omega(M_PI / 100.0, 0.0, 0.0);
  Vector3 acc(0.1, 0.0, 0.0);
  double dt = 10;
  Matrix9 aF;
  Matrix93 aG1, aG2;
  boost::function<NavState(const NavState&, const Vector3&, const Vector3&)> update =
      boost::bind(&NavState::update, _1, _2, _3, dt, boost::none,
          boost::none, boost::none);
  Vector3 b_acc = kAttitude * acc;
  NavState expected(kAttitude.expmap(dt * omega),
      kPosition + Point3((kVelocity + b_acc * dt / 2) * dt),
      kVelocity + b_acc * dt);
  NavState actual = kState1.update(acc, omega, dt, aF, aG1, aG2);
  EXPECT(assert_equal(expected, actual));
  EXPECT(assert_equal(numericalDerivative31(update, kState1, acc, omega, 1e-7), aF, 1e-7));
  EXPECT(assert_equal(numericalDerivative32(update, kState1, acc, omega, 1e-7), aG1, 1e-7));
  EXPECT(assert_equal(numericalDerivative33(update, kState1, acc, omega, 1e-7), aG2, 1e-7));

  // Try different values
  omega = Vector3(0.1, 0.2, 0.3);
  acc = Vector3(0.4, 0.5, 0.6);
  kState1.update(acc, omega, dt, aF, aG1, aG2);
  EXPECT(assert_equal(numericalDerivative31(update, kState1, acc, omega, 1e-7), aF, 1e-7));
  EXPECT(assert_equal(numericalDerivative32(update, kState1, acc, omega, 1e-7), aG1, 1e-7));
  EXPECT(assert_equal(numericalDerivative33(update, kState1, acc, omega, 1e-7), aG2, 1e-7));
}

/* ************************************************************************* */
static const double dt = 2.0;
boost::function<Vector9(const NavState&, const bool&)> coriolis = boost::bind(
    &NavState::coriolis, _1, dt, kOmegaCoriolis, _2, boost::none);

TEST(NavState, Coriolis) {
  Matrix9 aH;

  // first-order
  kState1.coriolis(dt, kOmegaCoriolis, false, aH);
  EXPECT(assert_equal(numericalDerivative21(coriolis, kState1, false), aH));
  // second-order
  kState1.coriolis(dt, kOmegaCoriolis, true, aH);
  EXPECT(assert_equal(numericalDerivative21(coriolis, kState1, true), aH));
}

TEST(NavState, Coriolis2) {
  Matrix9 aH;
  NavState state2(Rot3::RzRyRx(M_PI / 12.0, M_PI / 6.0, M_PI / 4.0),
      Point3(5.0, 1.0, -50.0), Vector3(0.5, 0.0, 0.0));

  // first-order
  state2.coriolis(dt, kOmegaCoriolis, false, aH);
  EXPECT(assert_equal(numericalDerivative21(coriolis, state2, false), aH));
  // second-order
  state2.coriolis(dt, kOmegaCoriolis, true, aH);
  EXPECT(assert_equal(numericalDerivative21(coriolis, state2, true), aH));
}

/* ************************************************************************* */
TEST(NavState, CorrectPIM) {
  Vector9 xi;
  xi << 0.1, 0.1, 0.1, 0.2, 0.3, 0.4, -0.1, -0.2, -0.3;
  double dt = 0.5;
  Matrix9 aH1, aH2;
  boost::function<Vector9(const NavState&, const Vector9&)> correctPIM =
      boost::bind(&NavState::correctPIM, _1, _2, dt, kGravity, kOmegaCoriolis,
          false, boost::none, boost::none);
  kState1.correctPIM(xi, dt, kGravity, kOmegaCoriolis, false, aH1, aH2);
  EXPECT(assert_equal(numericalDerivative21(correctPIM, kState1, xi), aH1));
  EXPECT(assert_equal(numericalDerivative22(correctPIM, kState1, xi), aH2));
}

/* ************************************************************************* */
int main() {
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}
/* ************************************************************************* */
