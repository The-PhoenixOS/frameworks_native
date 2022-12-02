/*
 * Copyright 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>

#include <EventHub.h>
#include <gestures/GestureConverter.h>
#include <gtest/gtest.h>

#include "FakeEventHub.h"
#include "FakeInputReaderPolicy.h"
#include "FakePointerController.h"
#include "InstrumentedInputReader.h"
#include "NotifyArgs.h"
#include "TestConstants.h"
#include "TestInputListener.h"
#include "TestInputListenerMatchers.h"
#include "include/gestures.h"
#include "ui/Rotation.h"

namespace android {

using testing::AllOf;

class GestureConverterTest : public testing::Test {
protected:
    static constexpr int32_t DEVICE_ID = END_RESERVED_ID + 1000;
    static constexpr int32_t EVENTHUB_ID = 1;
    static constexpr stime_t ARBITRARY_GESTURE_TIME = 1.2;
    static constexpr float POINTER_X = 100;
    static constexpr float POINTER_Y = 200;

    void SetUp() {
        mFakeEventHub = std::make_unique<FakeEventHub>();
        mFakePolicy = sp<FakeInputReaderPolicy>::make();
        mFakeListener = std::make_unique<TestInputListener>();
        mReader = std::make_unique<InstrumentedInputReader>(mFakeEventHub, mFakePolicy,
                                                            *mFakeListener);
        mDevice = newDevice();
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_MT_POSITION_X, -500, 500, 0, 0, 20);
        mFakeEventHub->addAbsoluteAxis(EVENTHUB_ID, ABS_MT_POSITION_Y, -500, 500, 0, 0, 20);

        mFakePointerController = std::make_shared<FakePointerController>();
        mFakePointerController->setBounds(0, 0, 800 - 1, 480 - 1);
        mFakePointerController->setPosition(POINTER_X, POINTER_Y);
        mFakePolicy->setPointerController(mFakePointerController);
    }

    std::shared_ptr<InputDevice> newDevice() {
        InputDeviceIdentifier identifier;
        identifier.name = "device";
        identifier.location = "USB1";
        identifier.bus = 0;
        std::shared_ptr<InputDevice> device =
                std::make_shared<InputDevice>(mReader->getContext(), DEVICE_ID, /* generation= */ 2,
                                              identifier);
        mReader->pushNextDevice(device);
        mFakeEventHub->addDevice(EVENTHUB_ID, identifier.name, InputDeviceClass::TOUCHPAD,
                                 identifier.bus);
        mReader->loopOnce();
        return device;
    }

    std::shared_ptr<FakeEventHub> mFakeEventHub;
    sp<FakeInputReaderPolicy> mFakePolicy;
    std::unique_ptr<TestInputListener> mFakeListener;
    std::unique_ptr<InstrumentedInputReader> mReader;
    std::shared_ptr<InputDevice> mDevice;
    std::shared_ptr<FakePointerController> mFakePointerController;
};

TEST_F(GestureConverterTest, Move) {
    InputDeviceContext deviceContext(*mDevice, EVENTHUB_ID);
    GestureConverter converter(*mReader->getContext(), deviceContext, DEVICE_ID);

    Gesture moveGesture(kGestureMove, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME, -5, 10);
    std::list<NotifyArgs> args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, moveGesture);
    ASSERT_EQ(1u, args.size());

    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_HOVER_MOVE),
                      WithCoords(POINTER_X - 5, POINTER_Y + 10), WithRelativeMotion(-5, 10),
                      WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER), WithButtonState(0),
                      WithPressure(0.0f)));

    ASSERT_NO_FATAL_FAILURE(mFakePointerController->assertPosition(95, 210));
}

TEST_F(GestureConverterTest, Move_Rotated) {
    InputDeviceContext deviceContext(*mDevice, EVENTHUB_ID);
    GestureConverter converter(*mReader->getContext(), deviceContext, DEVICE_ID);
    converter.setOrientation(ui::ROTATION_90);

    Gesture moveGesture(kGestureMove, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME, -5, 10);
    std::list<NotifyArgs> args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, moveGesture);
    ASSERT_EQ(1u, args.size());

    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_HOVER_MOVE),
                      WithCoords(POINTER_X + 10, POINTER_Y + 5), WithRelativeMotion(10, 5),
                      WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER), WithButtonState(0),
                      WithPressure(0.0f)));

    ASSERT_NO_FATAL_FAILURE(mFakePointerController->assertPosition(110, 205));
}

TEST_F(GestureConverterTest, ButtonsChange) {
    InputDeviceContext deviceContext(*mDevice, EVENTHUB_ID);
    GestureConverter converter(*mReader->getContext(), deviceContext, DEVICE_ID);

    // Press left and right buttons at once
    Gesture downGesture(kGestureButtonsChange, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME,
                        /* down= */ GESTURES_BUTTON_LEFT | GESTURES_BUTTON_RIGHT,
                        /* up= */ GESTURES_BUTTON_NONE, /* is_tap= */ false);
    std::list<NotifyArgs> args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, downGesture);
    ASSERT_EQ(3u, args.size());

    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_DOWN),
                      WithButtonState(AMOTION_EVENT_BUTTON_PRIMARY |
                                      AMOTION_EVENT_BUTTON_SECONDARY),
                      WithCoords(POINTER_X, POINTER_Y),
                      WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    args.pop_front();
    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_BUTTON_PRESS),
                      WithActionButton(AMOTION_EVENT_BUTTON_PRIMARY),
                      WithButtonState(AMOTION_EVENT_BUTTON_PRIMARY),
                      WithCoords(POINTER_X, POINTER_Y),
                      WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    args.pop_front();
    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_BUTTON_PRESS),
                      WithActionButton(AMOTION_EVENT_BUTTON_SECONDARY),
                      WithButtonState(AMOTION_EVENT_BUTTON_PRIMARY |
                                      AMOTION_EVENT_BUTTON_SECONDARY),
                      WithCoords(POINTER_X, POINTER_Y),
                      WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));

    // Then release the left button
    Gesture leftUpGesture(kGestureButtonsChange, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME,
                          /* down= */ GESTURES_BUTTON_NONE, /* up= */ GESTURES_BUTTON_LEFT,
                          /* is_tap= */ false);
    args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, leftUpGesture);
    ASSERT_EQ(1u, args.size());

    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_BUTTON_RELEASE),
                      WithActionButton(AMOTION_EVENT_BUTTON_PRIMARY),
                      WithButtonState(AMOTION_EVENT_BUTTON_SECONDARY),
                      WithCoords(POINTER_X, POINTER_Y),
                      WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));

    // Finally release the right button
    Gesture rightUpGesture(kGestureButtonsChange, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME,
                           /* down= */ GESTURES_BUTTON_NONE, /* up= */ GESTURES_BUTTON_RIGHT,
                           /* is_tap= */ false);
    args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, rightUpGesture);
    ASSERT_EQ(2u, args.size());

    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_BUTTON_RELEASE),
                      WithActionButton(AMOTION_EVENT_BUTTON_SECONDARY), WithButtonState(0),
                      WithCoords(POINTER_X, POINTER_Y),
                      WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    args.pop_front();
    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_UP), WithButtonState(0),
                      WithCoords(POINTER_X, POINTER_Y),
                      WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
}

TEST_F(GestureConverterTest, DragWithButton) {
    InputDeviceContext deviceContext(*mDevice, EVENTHUB_ID);
    GestureConverter converter(*mReader->getContext(), deviceContext, DEVICE_ID);

    // Press the button
    Gesture downGesture(kGestureButtonsChange, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME,
                        /* down= */ GESTURES_BUTTON_LEFT, /* up= */ GESTURES_BUTTON_NONE,
                        /* is_tap= */ false);
    std::list<NotifyArgs> args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, downGesture);
    ASSERT_EQ(2u, args.size());

    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_DOWN),
                      WithButtonState(AMOTION_EVENT_BUTTON_PRIMARY),
                      WithCoords(POINTER_X, POINTER_Y),
                      WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    args.pop_front();
    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_BUTTON_PRESS),
                      WithActionButton(AMOTION_EVENT_BUTTON_PRIMARY),
                      WithButtonState(AMOTION_EVENT_BUTTON_PRIMARY),
                      WithCoords(POINTER_X, POINTER_Y),
                      WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));

    // Move
    Gesture moveGesture(kGestureMove, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME, -5, 10);
    args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, moveGesture);
    ASSERT_EQ(1u, args.size());

    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_MOVE),
                      WithCoords(POINTER_X - 5, POINTER_Y + 10), WithRelativeMotion(-5, 10),
                      WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER),
                      WithButtonState(AMOTION_EVENT_BUTTON_PRIMARY), WithPressure(1.0f)));

    ASSERT_NO_FATAL_FAILURE(mFakePointerController->assertPosition(95, 210));

    // Release the button
    Gesture upGesture(kGestureButtonsChange, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME,
                      /* down= */ GESTURES_BUTTON_NONE, /* up= */ GESTURES_BUTTON_LEFT,
                      /* is_tap= */ false);
    args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, upGesture);
    ASSERT_EQ(2u, args.size());

    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_BUTTON_RELEASE),
                      WithActionButton(AMOTION_EVENT_BUTTON_PRIMARY), WithButtonState(0),
                      WithCoords(POINTER_X - 5, POINTER_Y + 10),
                      WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    args.pop_front();
    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_UP), WithButtonState(0),
                      WithCoords(POINTER_X - 5, POINTER_Y + 10),
                      WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
}

TEST_F(GestureConverterTest, ThreeFingerSwipe_ClearsClassificationAndOffsetsAfterGesture) {
    InputDeviceContext deviceContext(*mDevice, EVENTHUB_ID);
    GestureConverter converter(*mReader->getContext(), deviceContext, DEVICE_ID);

    Gesture startGesture(kGestureSwipe, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME, /* dx= */ 0,
                         /* dy= */ 0);
    std::list<NotifyArgs> args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, startGesture);

    Gesture liftGesture(kGestureSwipeLift, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME);
    args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, liftGesture);

    Gesture moveGesture(kGestureMove, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME, /* dx= */ -5,
                        /* dy= */ 10);
    args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, moveGesture);
    ASSERT_EQ(1u, args.size());
    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionClassification(MotionClassification::NONE),
                      WithGestureOffset(0, 0, EPSILON)));
}

TEST_F(GestureConverterTest, ThreeFingerSwipe_Vertical) {
    // The gestures library will "lock" a swipe into the dimension it starts in. For example, if you
    // start swiping up and then start moving left or right, it'll return gesture events with only Y
    // deltas until you lift your fingers and start swiping again. That's why each of these tests
    // only checks movement in one dimension.
    InputDeviceContext deviceContext(*mDevice, EVENTHUB_ID);
    GestureConverter converter(*mReader->getContext(), deviceContext, DEVICE_ID);

    Gesture startGesture(kGestureSwipe, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME, /* dx= */ 0,
                         /* dy= */ 10);
    std::list<NotifyArgs> args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, startGesture);
    ASSERT_EQ(4u, args.size());

    // Three fake fingers should be created. We don't actually care where they are, so long as they
    // move appropriately.
    NotifyMotionArgs arg = std::get<NotifyMotionArgs>(args.front());
    ASSERT_THAT(arg,
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_DOWN), WithGestureOffset(0, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(1u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    PointerCoords finger0Start = arg.pointerCoords[0];
    args.pop_front();
    arg = std::get<NotifyMotionArgs>(args.front());
    ASSERT_THAT(arg,
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_POINTER_DOWN |
                                       1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
                      WithGestureOffset(0, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(2u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    PointerCoords finger1Start = arg.pointerCoords[1];
    args.pop_front();
    arg = std::get<NotifyMotionArgs>(args.front());
    ASSERT_THAT(arg,
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_POINTER_DOWN |
                                       2 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
                      WithGestureOffset(0, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(3u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    PointerCoords finger2Start = arg.pointerCoords[2];
    args.pop_front();

    arg = std::get<NotifyMotionArgs>(args.front());
    ASSERT_THAT(arg,
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_MOVE),
                      WithGestureOffset(0, -0.01, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(3u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    EXPECT_EQ(arg.pointerCoords[0].getX(), finger0Start.getX());
    EXPECT_EQ(arg.pointerCoords[1].getX(), finger1Start.getX());
    EXPECT_EQ(arg.pointerCoords[2].getX(), finger2Start.getX());
    EXPECT_EQ(arg.pointerCoords[0].getY(), finger0Start.getY() - 10);
    EXPECT_EQ(arg.pointerCoords[1].getY(), finger1Start.getY() - 10);
    EXPECT_EQ(arg.pointerCoords[2].getY(), finger2Start.getY() - 10);

    Gesture continueGesture(kGestureSwipe, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME,
                            /* dx= */ 0, /* dy= */ 5);
    args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, continueGesture);
    ASSERT_EQ(1u, args.size());
    arg = std::get<NotifyMotionArgs>(args.front());
    ASSERT_THAT(arg,
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_MOVE),
                      WithGestureOffset(0, -0.005, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(3u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    EXPECT_EQ(arg.pointerCoords[0].getX(), finger0Start.getX());
    EXPECT_EQ(arg.pointerCoords[1].getX(), finger1Start.getX());
    EXPECT_EQ(arg.pointerCoords[2].getX(), finger2Start.getX());
    EXPECT_EQ(arg.pointerCoords[0].getY(), finger0Start.getY() - 15);
    EXPECT_EQ(arg.pointerCoords[1].getY(), finger1Start.getY() - 15);
    EXPECT_EQ(arg.pointerCoords[2].getY(), finger2Start.getY() - 15);

    Gesture liftGesture(kGestureSwipeLift, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME);
    args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, liftGesture);
    ASSERT_EQ(3u, args.size());
    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_POINTER_UP |
                                       2 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
                      WithGestureOffset(0, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(3u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    args.pop_front();
    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_POINTER_UP |
                                       1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
                      WithGestureOffset(0, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(2u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    args.pop_front();
    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_UP), WithGestureOffset(0, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(1u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
}

TEST_F(GestureConverterTest, FourFingerSwipe_Horizontal) {
    InputDeviceContext deviceContext(*mDevice, EVENTHUB_ID);
    GestureConverter converter(*mReader->getContext(), deviceContext, DEVICE_ID);

    Gesture startGesture(kGestureFourFingerSwipe, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME,
                         /* dx= */ 10, /* dy= */ 0);
    std::list<NotifyArgs> args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, startGesture);
    ASSERT_EQ(5u, args.size());

    // Four fake fingers should be created. We don't actually care where they are, so long as they
    // move appropriately.
    NotifyMotionArgs arg = std::get<NotifyMotionArgs>(args.front());
    ASSERT_THAT(arg,
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_DOWN), WithGestureOffset(0, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(1u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    PointerCoords finger0Start = arg.pointerCoords[0];
    args.pop_front();
    arg = std::get<NotifyMotionArgs>(args.front());
    ASSERT_THAT(arg,
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_POINTER_DOWN |
                                       1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
                      WithGestureOffset(0, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(2u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    PointerCoords finger1Start = arg.pointerCoords[1];
    args.pop_front();
    arg = std::get<NotifyMotionArgs>(args.front());
    ASSERT_THAT(arg,
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_POINTER_DOWN |
                                       2 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
                      WithGestureOffset(0, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(3u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    PointerCoords finger2Start = arg.pointerCoords[2];
    args.pop_front();
    arg = std::get<NotifyMotionArgs>(args.front());
    ASSERT_THAT(arg,
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_POINTER_DOWN |
                                       3 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
                      WithGestureOffset(0, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(4u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    PointerCoords finger3Start = arg.pointerCoords[3];
    args.pop_front();

    arg = std::get<NotifyMotionArgs>(args.front());
    ASSERT_THAT(arg,
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_MOVE),
                      WithGestureOffset(0.01, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(4u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    EXPECT_EQ(arg.pointerCoords[0].getX(), finger0Start.getX() + 10);
    EXPECT_EQ(arg.pointerCoords[1].getX(), finger1Start.getX() + 10);
    EXPECT_EQ(arg.pointerCoords[2].getX(), finger2Start.getX() + 10);
    EXPECT_EQ(arg.pointerCoords[3].getX(), finger3Start.getX() + 10);
    EXPECT_EQ(arg.pointerCoords[0].getY(), finger0Start.getY());
    EXPECT_EQ(arg.pointerCoords[1].getY(), finger1Start.getY());
    EXPECT_EQ(arg.pointerCoords[2].getY(), finger2Start.getY());
    EXPECT_EQ(arg.pointerCoords[3].getY(), finger3Start.getY());

    Gesture continueGesture(kGestureFourFingerSwipe, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME,
                            /* dx= */ 5, /* dy= */ 0);
    args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, continueGesture);
    ASSERT_EQ(1u, args.size());
    arg = std::get<NotifyMotionArgs>(args.front());
    ASSERT_THAT(arg,
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_MOVE),
                      WithGestureOffset(0.005, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(4u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    EXPECT_EQ(arg.pointerCoords[0].getX(), finger0Start.getX() + 15);
    EXPECT_EQ(arg.pointerCoords[1].getX(), finger1Start.getX() + 15);
    EXPECT_EQ(arg.pointerCoords[2].getX(), finger2Start.getX() + 15);
    EXPECT_EQ(arg.pointerCoords[3].getX(), finger3Start.getX() + 15);
    EXPECT_EQ(arg.pointerCoords[0].getY(), finger0Start.getY());
    EXPECT_EQ(arg.pointerCoords[1].getY(), finger1Start.getY());
    EXPECT_EQ(arg.pointerCoords[2].getY(), finger2Start.getY());
    EXPECT_EQ(arg.pointerCoords[3].getY(), finger3Start.getY());

    Gesture liftGesture(kGestureSwipeLift, ARBITRARY_GESTURE_TIME, ARBITRARY_GESTURE_TIME);
    args = converter.handleGesture(ARBITRARY_TIME, READ_TIME, liftGesture);
    ASSERT_EQ(4u, args.size());
    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_POINTER_UP |
                                       3 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
                      WithGestureOffset(0, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(4u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    args.pop_front();
    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_POINTER_UP |
                                       2 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
                      WithGestureOffset(0, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(3u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    args.pop_front();
    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_POINTER_UP |
                                       1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT),
                      WithGestureOffset(0, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(2u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
    args.pop_front();
    ASSERT_THAT(std::get<NotifyMotionArgs>(args.front()),
                AllOf(WithMotionAction(AMOTION_EVENT_ACTION_UP), WithGestureOffset(0, 0, EPSILON),
                      WithMotionClassification(MotionClassification::MULTI_FINGER_SWIPE),
                      WithPointerCount(1u), WithToolType(AMOTION_EVENT_TOOL_TYPE_FINGER)));
}

} // namespace android
