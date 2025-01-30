#include "opencv2/opencv.hpp"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "zmq.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>

// ���b�Z�[�W���M��Ƀ�������������郁�\�b�h
void my_free(void* data, void* hint)
{
    free(data);
}

// �摜���擾����ZMQ�ő��M���郁�\�b�h
void send_image(cv::VideoCapture& cap, zmq::socket_t& socket, int command, bool camera_connected)
{
    try {
        cv::Mat image; // �擾�����t���[��

        // �J�����ڑ���Ԃ𑗐M
        zmq::message_t camera_connected_msg(&camera_connected, sizeof(camera_connected));
        socket.send(camera_connected_msg, zmq::send_flags::sndmore);

        // �t���[�����擾
        if (cap.read(image)) {
            // �摜���
            int32_t info[2];
            info[0] = static_cast<int32_t>(image.rows);
            info[1] = static_cast<int32_t>(image.cols);

            // �R�}���h�𑗐M
            zmq::message_t command_msg(&command, sizeof(command));
            socket.send(command_msg, zmq::send_flags::sndmore);

            // �摜���𑗐M
            for (int i = 0; i < 2; i++) {
                zmq::message_t msg(&info[i], sizeof(int32_t), nullptr);
                socket.send(msg, zmq::send_flags::sndmore);
            }

            // �s�N�Z���f�[�^�𑗐M
            std::vector<uchar> data(image.total() * image.elemSize());
            memcpy(data.data(), image.data, data.size());
			std::cout << "data size: " << data.size() << std::endl;

            zmq::message_t msg2(data.data(), data.size(), nullptr);
            socket.send(msg2, zmq::send_flags::none);

            // C#����̕ԐM����M
            zmq::message_t reply;
            socket.recv(reply, zmq::recv_flags::none);
            std::string reply_str(static_cast<char*>(reply.data()), reply.size());
        }
        else {
            std::cerr << "�t���[���̎擾�Ɏ��s���܂���" << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "send_image�֐����ŗ�O���������܂���: " << e.what() << std::endl;
    }
}

// ���C���֐�
int main(int argc, char* argv[]) {
    // ZMQ�̃R���e�L�X�g�ƃ\�P�b�g���쐬
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REQ);
    zmq::socket_t control_socket(context, ZMQ_PULL);

    try {
        std::cout << "�v���O�����J�n" << std::endl;

        // ZMQ�R�l�N�V�������쐬
        socket.connect("tcp://localhost:5555");
        std::cout << "localhost:5555�ɐڑ����܂���" << std::endl;

        control_socket.bind("tcp://localhost:5557");
        std::cout << "localhost:5557�Ƀo�C���h���܂���" << std::endl;

        std::atomic<bool> is_running(true); // �v���W�F�N�g�i�s���t���O
        std::atomic<bool> is_sending(false); // c#�ɉ摜�𑗂�t���O
        int command;
        std::string selected_camera;
		bool camera_connected = false;
        cv::VideoCapture cap; // �J�����f�o�C�X

        std::thread control_thread([&]() {
            while (is_running) {
                // C#����̃t���O�ɑΉ�
                zmq::message_t message;
                control_socket.recv(message, zmq::recv_flags::none);
                command = *static_cast<int*>(message.data());
                std::cout << "���b�Z�[�W����M���܂���: " << command << std::endl;

                if (command == 1) { // start
                    zmq::message_t camera_message;
                    control_socket.recv(camera_message, zmq::recv_flags::none);
                    selected_camera = std::string(static_cast<char*>(camera_message.data()), camera_message.size());
                    std::cout << "�I�����ꂽ�J����: " << selected_camera << std::endl;

                    // �J�����f�o�C�X��������
                    int camera_index = (selected_camera == "camera 0") ? 0 : -1;
                    cap.open(camera_index, cv::CAP_DSHOW);
                    if (!cap.isOpened()) {
                        std::cerr << "�J�����f�o�C�X��������܂���" << std::endl;
						camera_connected = false;
                    }
                    else
					{
						camera_connected = true;
					}

                    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
                    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
                    cap.set(cv::CAP_PROP_FPS, 30);

                    is_sending = true;
                }
                else if (command == 0) { // stop
                    is_sending = false;
                }
                else if (command == -1) { // exit
                    is_sending = false;

                    std::cout << "5555�|�[�g�ɏI���R�}���h�𑗐M���܂��D" << std::endl;
                    // 5555�|�[�g�ɏI���R�}���h�𑗐M
                    send_image(cap, socket, command, camera_connected);

                    // �v���O�����I�����̏���
                    std::cout << "�v���O�����I��" << std::endl;

                    // �|�[�g�����
                    std::cout << "�\�P�b�g����܂�..." << std::endl;
                    socket.close();
                    control_socket.close();
                    context.close();
                    std::cout << "�\�P�b�g�������܂����B" << std::endl;

                    is_running = false;
                }
            }
            });

        try {
            while (is_running) {
                // Python�ɉ摜�ƃt���O�𑗂�
                if (is_sending) {
                    send_image(cap, socket, command, camera_connected);
                }
                // �K�؂ȃt���[�����[�g���ێ����邽�߂ɑҋ@
                cv::waitKey(10);
            }
        }
        catch (const std::exception& e) {
            std::cerr << "ZMQ�̃G���[���������܂���: " << e.what() << std::endl;
            return -1;
        }

        control_thread.join();
    }
    catch (const std::exception& e) {
        std::cerr << "��O���������܂���: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
