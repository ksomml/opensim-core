/* -------------------------------------------------------------------------- *
 *                          OpenSim:  testFrames.cpp                          *
 * -------------------------------------------------------------------------- *
 * The OpenSim API is a toolkit for musculoskeletal modeling and simulation.  *
 * See http://opensim.stanford.edu and the NOTICE file for more information.  *
 * OpenSim is developed at Stanford University and supported by the US        *
 * National Institutes of Health (U54 GM072970, R24 HD065690) and by DARPA    *
 * through the Warrior Web program.                                           *
 *                                                                            *
 * Copyright (c) 2005-2012 Stanford University and the Authors                *
 * Author(s): Ayman Habib                                                     *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may    *
 * not use this file except in compliance with the License. You may obtain a  *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0.         *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 * -------------------------------------------------------------------------- */

//==============================================================================
//
//  Tests Include:
//      1. Body
//      2. PhysicalOffsetFrame
//      
//     Add tests here as Frames are added to OpenSim
//
//==============================================================================
#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/Model/OffsetFrame.h>
#include <OpenSim/Auxiliary/auxiliaryTestFunctions.h>

using namespace OpenSim;
using namespace std;

void testBody();
void testOffsetFrameOnBody();
void testOffsetFrameOnBodySerialize();
void testOffsetFrameOnOffsetFrame();
void testStationOnFrame();

int main()
{
    SimTK::Array_<std::string> failures;

    try { testBody(); }
    catch (const std::exception& e){
        cout << e.what() <<endl; failures.push_back("testBody");
    }
        
    try { testOffsetFrameOnBody(); }
    catch (const std::exception& e){
        cout << e.what() <<endl; failures.push_back("testOffsetFrameOnBody");
    }

    try { testOffsetFrameOnBodySerialize(); }
    catch (const std::exception& e){
        cout << e.what() << endl; 
        failures.push_back("testOffsetFrameOnBodySerialize");
    }
    
    try { testOffsetFrameOnOffsetFrame(); }
    catch (const std::exception& e){
        cout << e.what() << endl;
        failures.push_back("testOffsetFrameOnOffsetFrame");
    }

    try { testStationOnFrame(); }
    catch (const std::exception& e){
        cout << e.what() << endl; failures.push_back("testStationOnFrame");
    }

    if (!failures.empty()) {
        cout << "Done, with failure(s): " << failures << endl;
        return 1;
    }

    cout << "Done. All cases passed." << endl;

    return 0;
}

//==============================================================================
// Test Cases
//==============================================================================

void testBody()
{
    cout << "Running testBody" << endl;
    Model* pendulum = new Model("double_pendulum.osim");
    const OpenSim::Body& rod1 = pendulum->getBodySet().get("rod1");
    SimTK::State& st = pendulum->initSystem();
    for (double ang = 0; ang <= 90.0; ang += 10.){
        double radAngle = SimTK::convertDegreesToRadians(ang);
        const Coordinate& coord = pendulum->getCoordinateSet().get("q1");
        coord.setValue(st, radAngle);
        const SimTK::Transform& xform = rod1.getGroundTransform(st);
        // The transform should give a translation of .353553, .353553, 0.0
        SimTK::Vec3 p_known(0.5*sin(radAngle), -0.5*cos(radAngle), 0.0);
        ASSERT_EQUAL(p_known, xform.p(), SimTK::Vec3(SimTK::Eps),
            __FILE__, __LINE__,
            "testBody(): incorrect rod1 location in ground.");
        // The rotation part is a pure bodyfixed Z-rotation by radAngle.
        SimTK::Vec3 angles = xform.R().convertRotationToBodyFixedXYZ();
        SimTK::Vec3 angs_known(0, 0, radAngle);
        ASSERT_EQUAL(angs_known, angles, SimTK::Vec3(SimTK::Eps), 
            __FILE__, __LINE__,
            "testBody(): incorrect rod1 orientation in ground.");
    }
}

void testOffsetFrameOnBody()
{
    SimTK::Vec3 tolerance(SimTK::Eps);

    cout << "Running testOffsetFrameOnBody" << endl;
    Model* pendulum = new Model("double_pendulum.osim");
    const OpenSim::Body& rod1 = pendulum->getBodySet().get("rod1");

    // The offset transform on the rod body
    SimTK::Transform X_RO;
    // offset position by some random vector
    X_RO.setP(SimTK::Vec3(1.2, 2.5, 3.3));
    // rotate the frame by some non-planar rotation
    SimTK::Vec3 angs_known(0.33, 0.22, 0.11);
    X_RO.updR().setRotationToBodyFixedXYZ(angs_known);
    PhysicalOffsetFrame* offsetFrame = new PhysicalOffsetFrame(rod1, X_RO);
    pendulum->addFrame(offsetFrame);
    SimTK::State& s = pendulum->initSystem();
    const SimTK::Transform& X_GR = rod1.getGroundTransform(s);
    const SimTK::Transform& X_GO = offsetFrame->getGroundTransform(s);

    // Compute the offset transform based on frames expressed in ground
    SimTK::Transform X_RO_2 = ~X_GR*X_GO;
    SimTK::Vec3 angles = X_RO_2.R().convertRotationToBodyFixedXYZ();

    // Offsets should be identical expressed in ground or in the Body
    ASSERT_EQUAL(X_RO.p(), X_RO_2.p(), tolerance,
        __FILE__, __LINE__, 
        "testOffsetFrameOnBody(): incorrect expression of offset in ground.");
    ASSERT_EQUAL(angs_known, angles, tolerance,
        __FILE__, __LINE__,
        "testOffsetFrameOnBody(): incorrect expression of offset in ground.");
    // make sure that this OffsetFrame knows that it is rigidly fixed to the
    // same MobilizedBody as Body rod1
    ASSERT(rod1.getMobilizedBodyIndex() == offsetFrame->getMobilizedBodyIndex(),
        __FILE__, __LINE__, 
        "testOffsetFrameOnBody(): incorrect MobilizedBodyIndex");

    Transform X_RO_3 = offsetFrame->findTransformBetween(s, rod1);
    SimTK::Vec3 angles3 = X_RO_3.R().convertRotationToBodyFixedXYZ();
    // Transform should be identical to the original offset 
    ASSERT_EQUAL(X_RO.p(), X_RO_3.p(), tolerance,
        __FILE__, __LINE__,
        "testOffsetFrameOnBody(): incorrect transform between offset and rod.");
    ASSERT_EQUAL(angs_known, angles3, tolerance,
        __FILE__, __LINE__,
        "testOffsetFrameOnBody(): incorrect transform between offset and rod.");

    SimTK::Vec3 f_R(10.1, 20.2, 30.3);
    SimTK::Vec3 f_RG = rod1.expressVectorInAnotherFrame(s, f_R,
        pendulum->getGroundBody());

    ASSERT_EQUAL(f_R.norm(), f_RG.norm(), tolerance(0),
        __FILE__, __LINE__,
        "testOffsetFrameOnBody(): incorrect re-expression of vector.");

    SimTK::Vec3 f_RO = rod1.expressVectorInAnotherFrame(s, f_R, *offsetFrame);
    ASSERT_EQUAL(f_R.norm(), f_RO.norm(), tolerance(0),
        __FILE__, __LINE__,
        "testOffsetFrameOnBody(): incorrect re-expression of vector.");

    SimTK::Vec3 p_R(0.333, 0.222, 0.111);
    SimTK::Vec3 p_G = 
        rod1.findLocationInAnotherFrame(s, p_R, pendulum->getGroundBody());
    SimTK::Vec3 p_G_2 = 
        rod1.getMobilizedBody().findStationLocationInGround(s, p_R);

    ASSERT_EQUAL(p_G_2, p_G, tolerance,
        __FILE__, __LINE__,
        "testOffsetFrameOnBody(): incorrect point location in ground.");
}

void testOffsetFrameOnOffsetFrame()
{
    SimTK::Vec3 tolerance(SimTK::Eps);

    cout << "Running testOffsetFrameOnOffsetFrame" << endl;
    Model* pendulum = new Model("double_pendulum.osim");
    const OpenSim::Body& rod1 = pendulum->getBodySet().get("rod1");
    
    SimTK::Transform X_RO;
    //offset position by some random vector
    X_RO.setP(SimTK::Vec3(1.2, 2.5, 3.3));
    // rotate the frame 
    X_RO.updR().setRotationToBodyFixedXYZ(SimTK::Vec3(0.33, 0.22, 0.11));
    PhysicalOffsetFrame* offsetFrame = new PhysicalOffsetFrame(rod1, X_RO);
    pendulum->addFrame(offsetFrame);

    //connect a second frame to the first OffsetFrame without any offset
    PhysicalOffsetFrame* secondFrame = offsetFrame->clone();
    secondFrame->setParentFrame(*offsetFrame);
    X_RO.setP(SimTK::Vec3(3.3, 2.2, 1.1));
    X_RO.updR().setRotationToBodyFixedXYZ(SimTK::Vec3(1.5, -0.707, 0.5));
    secondFrame->setOffsetTransform(X_RO);
    pendulum->addFrame(secondFrame);

    SimTK::State& s = pendulum->initSystem();

    const Frame& base = secondFrame->findBaseFrame();
    SimTK::Transform XinBase = secondFrame->findTransformInBaseFrame();

    const SimTK::Transform& X_GR = rod1.getGroundTransform(s);
    const SimTK::Transform& X_GO = secondFrame->getGroundTransform(s);

    SimTK::Vec3 angs_known = XinBase.R().convertRotationToBodyFixedXYZ();

    // Compute the offset of these frames in ground
    SimTK::Transform X_RO_2 = ~X_GR*X_GO;
    SimTK::Vec3 angles = X_RO_2.R().convertRotationToBodyFixedXYZ();

    // Offsets should be identical expressed in ground or in the Body
    ASSERT_EQUAL(XinBase.p(), X_RO_2.p(), tolerance,
        __FILE__, __LINE__, 
        "testOffsetFrameOnOffsetFrame(): incorrect expression of offset in ground.");
    ASSERT_EQUAL(angs_known, angles, tolerance,
        __FILE__, __LINE__, 
        "testOffsetFrameOnOffsetFrame(): incorrect expression of offset in ground.");

    // make sure that this OffsetFrame knows that it is rigidly fixed to the
    // same MobilizedBody as Body rod1
    ASSERT(rod1.getMobilizedBodyIndex() == secondFrame->getMobilizedBodyIndex(),
        __FILE__, __LINE__, 
        "testOffsetFrameOnOffsetFrame(): incorrect MobilizedBodyIndex");

    // test base Frames are identical
    const Frame& baseRod = rod1.findBaseFrame();
    ASSERT(base == baseRod, __FILE__, __LINE__, 
        "testOffsetFrameOnOffsetFrame(): incorrect base frame for OffsetFrame");
    const Frame& base1 = offsetFrame->findBaseFrame();
    ASSERT(base1 == base, __FILE__, __LINE__,
        "testOffsetFrameOnOffsetFrame(): incorrect base frames for OffsetFrame");
}

void testOffsetFrameOnBodySerialize()
{
    SimTK::Vec3 tolerance(SimTK::Eps);

    cout << "Running testOffsetFrameOnBodySerialize" << endl;
    Model* pendulum = new Model("double_pendulum.osim");
    const OpenSim::Body& rod1 = pendulum->getBodySet().get("rod1");

    SimTK::Transform X_RO;
    X_RO.setP(SimTK::Vec3(0.0, .5, 0.0));
    X_RO.updR().setRotationFromAngleAboutAxis(SimTK::Pi/4.0, SimTK::ZAxis);

    PhysicalOffsetFrame* offsetFrame = new PhysicalOffsetFrame(rod1, X_RO);
    offsetFrame->setName("myExtraFrame");
    pendulum->addFrame(offsetFrame);

    SimTK::State& s1 = pendulum->initSystem();
    const SimTK::Transform& X_GO_1 = offsetFrame->getGroundTransform(s1);
    pendulum->print("double_pendulum_extraFrame.osim");
    // now read the model from file
    Model* pendulumWFrame = new Model("double_pendulum_extraFrame.osim");
    SimTK::State& s2 = pendulumWFrame->initSystem();
    ASSERT(*pendulum == *pendulumWFrame);

    const PhysicalFrame& myExtraFrame =
        dynamic_cast<const PhysicalFrame&>(pendulumWFrame->getComponent("myExtraFrame"));
    ASSERT(*offsetFrame == myExtraFrame);

    const SimTK::Transform& X_GO_2 = myExtraFrame.getGroundTransform(s2);
    ASSERT_EQUAL(X_GO_2.p(), X_GO_1.p(), tolerance, __FILE__, __LINE__,
        "testOffsetFrameOnBodySerialize(): incorrect expression of offset in ground.");
    ASSERT_EQUAL(X_GO_2.R().convertRotationToBodyFixedXYZ(), 
        X_GO_1.R().convertRotationToBodyFixedXYZ(), tolerance,
        __FILE__, __LINE__,
        "testOffsetFrameOnBodySerialize(): incorrect expression of offset in ground.");
    // verify that OffsetFrame shares the same underlying MobilizedBody as rod1
    ASSERT(rod1.getMobilizedBodyIndex() == myExtraFrame.getMobilizedBodyIndex(),
        __FILE__, __LINE__,
        "testOffsetFrameOnBodySerialize(): incorrect MobilizedBodyIndex");
}

void testStationOnFrame()
{
    SimTK::Vec3 tolerance(SimTK::Eps);

    cout << "Running testStationOnFrame" << endl;

    Model* pendulum = new Model("double_pendulum.osim");
    // Get "rod1" frame
    const OpenSim::Body& rod1 = pendulum->getBodySet().get("rod1");
    const SimTK::Vec3& com = rod1.get_mass_center();
    // Create station aligned with rod1 com in rod1_frame
    Station* myStation = new Station();
    myStation->set_location(com);
    myStation->updConnector<PhysicalFrame>("reference_frame")
        .set_connected_to_name("rod1");
    pendulum->addModelComponent(myStation);
    // myStation should coinicde with com location of rod1 in ground
    SimTK::State& s = pendulum->initSystem();
    for (double ang = 0; ang <= 90.0; ang += 10.){
        double radAngle = SimTK::convertDegreesToRadians(ang);
        const Coordinate& coord = pendulum->getCoordinateSet().get("q1");
        coord.setValue(s, radAngle);

        SimTK::Vec3 comInGround = 
            myStation->findLocationInFrame(s, pendulum->getGroundBody());
        SimTK::Vec3 comBySimbody = 
            rod1.getMobilizedBody().findStationLocationInGround(s, com);
        ASSERT_EQUAL(comInGround, comBySimbody, tolerance, __FILE__, __LINE__,
            "testStationOnFrame(): failed to resolve station psoition in ground.");
    }
}
