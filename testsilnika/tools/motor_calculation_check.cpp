#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

struct MotorCommand {
    std::string mode;
    int pwm;
};

struct DriveCommand {
    MotorCommand left;
    MotorCommand right;
};

DriveCommand calculate_drive(int pwm_value, int direction_value) {
    if (pwm_value > 100) pwm_value = 100;
    if (pwm_value < -100) pwm_value = -100;
    if (direction_value > 100) direction_value = 100;
    if (direction_value < -100) direction_value = -100;

    if (pwm_value == 0) {
        return {{"COAST", 0}, {"COAST", 0}};
    }

    int pwm_magnitude = std::abs(pwm_value);
    bool forward = pwm_value > 0;
    int left_pwm = pwm_magnitude;
    int right_pwm = pwm_magnitude;
    std::string left_mode = forward ? "FORWARD" : "BACKWARD";
    std::string right_mode = left_mode;

    if (direction_value > 0) {
        right_pwm = ((100 - 2 * direction_value) * pwm_magnitude) / 100;
        right_mode = right_pwm == 0 ? "COAST" : right_pwm > 0 ? left_mode : (forward ? "BACKWARD" : "FORWARD");
        right_pwm = std::abs(right_pwm);
    } else if (direction_value < 0) {
        left_pwm = ((100 + 2 * direction_value) * pwm_magnitude) / 100;
        left_mode = left_pwm == 0 ? "COAST" : left_pwm > 0 ? left_mode : (forward ? "BACKWARD" : "FORWARD");
        left_pwm = std::abs(left_pwm);
    }

    return {{left_mode, left_pwm}, {right_mode, right_pwm}};
}

int main() {
    struct TestCase {
        int pwm;
        int direction;
        DriveCommand expected;
    };

    std::vector<TestCase> tests = {
        {50, 0, {{"FORWARD", 50}, {"FORWARD", 50}}},
        {50, 20, {{"FORWARD", 50}, {"FORWARD", 30}}},
        {50, -20, {{"FORWARD", 30}, {"FORWARD", 50}}},
        {-50, 0, {{"BACKWARD", 50}, {"BACKWARD", 50}}},
        {0, 0, {{"COAST", 0}, {"COAST", 0}}}
    };

    bool all_passed = true;

    for (const TestCase &test : tests) {
        DriveCommand actual = calculate_drive(test.pwm, test.direction);
        bool passed = actual.left.mode == test.expected.left.mode &&
                      actual.left.pwm == test.expected.left.pwm &&
                      actual.right.mode == test.expected.right.mode &&
                      actual.right.pwm == test.expected.right.pwm;

        std::cout << "Input pwm=" << test.pwm
                  << ", direction=" << test.direction << "\n"
                  << "  left:  " << actual.left.mode << ", " << actual.left.pwm << "%\n"
                  << "  right: " << actual.right.mode << ", " << actual.right.pwm << "%\n"
                  << "  result: " << (passed ? "PASS" : "FAIL") << "\n\n";

        all_passed = all_passed && passed;
    }

    return all_passed ? 0 : 1;
}
