#include <iostream>
#include <cstdlib>
#include <tuple>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <future>
#include <algorithm>
#include <vector>
#include <fstream>
#include <cstring>
#include "rapidxml.hpp"

using namespace std;
using namespace rapidxml;

class Game
{
public:
    int id_uniq = -1;
    int *clients;
    int *scores;
    vector<std::string> users;
    int client_capacity;
    int client_number;
    bool end_game = false;

    Game()
    {
        id_uniq = idGenerator();
        client_number = 0;
        client_capacity = 1;
        clients = new int[1];
        scores = new int[1];
        cout << "[Game.h] The game" << id_uniq << " is created!" << endl;
    }
    ~Game()
    {
        if (end_game)
        {
            cout << "[Game.h] The game was destructed!" << endl;
            delete[] clients;
            delete[] scores;
        }
    }
    int idGenerator()
    {
        srand(time(0));
        id_uniq = rand() % 10000;
        return id_uniq;
    }
    void Add_client(int fd_client, string username)
    {
        if (client_number == client_capacity)
        {
            client_capacity *= 2;
            int *new_clients = new int[client_capacity];
            for (int i = 0; i < client_number; i++)
            {
                new_clients[i] = clients[i];
            }
            delete[] clients;
            clients = new_clients;
        }
        scores[client_number] = 0;
        clients[client_number] = fd_client;
        users.push_back(username);
        client_number++;
    }
    bool Remove_client(int fd_client)
    {
        for (int i = 0; i <= client_number; i++)
        {
            if (clients[i] == fd_client)
            {
                clients[i] = -clients[i];
                users.erase(users.begin() + i);
                scores[i] = 0;
                client_number--;
                cout << "[Remove_client] Removed!" << endl;
                return true;
            }
        }
        return false;
    }
    void showClients()
    {
        cout << "Room id: " << id_uniq << endl;
        for (int i = 0; i < client_number; i++)
        {
            cout << "Client number " << i + 1 << " is: " << clients[i] << endl;
        }
    }
    void handleGame(int fd)
    {
        // int fd = *((int *)arg);
        char question[500];
        char response[255];
        char correct_answer[255];
        char score[255];
        int this_score;
        xml_document<> doc;
        xml_node<> *root_node;

        ifstream myFile("Q&A.xml");
        vector<char> contents((istreambuf_iterator<char>(myFile)), istreambuf_iterator<char>());
        contents.push_back('\0');

        doc.parse<0>(&contents[0]);

        root_node = doc.first_node("questionList");

        for (xml_node<> *question_node = root_node->first_node("question");
             question_node;
             question_node = question_node->next_sibling())
        {
            bzero(question, 500);
            strcat(question, question_node->value());
            strcat(question, "\n");
            int i = 0;
            for (xml_node<> *answer_node = question_node->first_node("answer");
                 answer_node;
                 answer_node = answer_node->next_sibling())
            {
                i++;
                strcat(question, answer_node->value());
                strcat(question, "\n");
                if (strcmp(answer_node->first_attribute("type")->value(), "true") == 0)
                {
                    strcpy(correct_answer, answer_node->value());
                    correct_answer[strlen(correct_answer)] = '\0';
                }
            }
            if (write(fd, question, strlen(question)) < 0)
            {
                perror("[Game Thread] Error at write() to client.\n");
            }

            bzero(response, 255);
            /* startingnow */
            if (read(fd, response, sizeof(response)) < 0)
            {
                perror("[Game Thread] Error at read() from client.\n");
            }
            if (strcmp(response, "disconnect") == 0)
            {
                Remove_client(fd);
                cout << "[Game Thread] Client with fd " << fd << " disconnected from the QUIZZGAME!" << endl;
                pthread_exit(NULL);
            }

            this_thread::sleep_for(chrono::seconds(15));

            bzero(response, 255);
            if (read(fd, response, sizeof(response)) < 0)
            {
                perror("[Game Thread] Error at read() from client.\n");
            }
            if (strcmp(response, "disconnect") == 0)
            {
                Remove_client(fd);
                cout << "[Game Thread] Client with fd " << fd << " disconnected from the QUIZZGAME!" << endl;
                pthread_exit(NULL);
            }
            response[strcspn(response, "\n")] = '\0';
            printf("[Game Thread] The client's %d answer is: %s\n", fd, response);
            bzero(question, 500);
            if (strcmp(response, correct_answer) == 0)
            {
                strcat(question, "\033[1;34mCorrect! +10points\033[1;0m\n");
                for (int i = 0; i < client_number; i++)
                {
                    if (clients[i] == fd)
                    {
                        scores[i] += 10;
                        this_score = scores[i];
                    }
                }
            }
            else
            {
                strcat(question, "\033[1;34mIncorrect! +0points\nThe correct answer was: \033[1;0m");
                strcat(question, correct_answer);
                strcat(question, "\n");
            }
            if (write(fd, question, strlen(question)) < 0)
            {
                perror("[Game Thread] Error at write() to client.\n");
            }
        }
        /* calculam si trimitem clasamentul */
        bzero(score, 255);
        snprintf(score, sizeof(score), "Congrats! Your score is %d", this_score);
        if (write(fd, score, strlen(score)) < 0)
        {
            perror("[Game Thread] Error at write() to client.\n");
        }
        bzero(response, 255);
        if (read(fd, response, sizeof(response)) < 0)
        {
            perror("[Game Thread] Error at read() from client.\n");
        }
        if (strcmp(response, "disconnect") == 0)
        {
            Remove_client(fd);
            cout << "[Game Thread] Client with fd " << fd << " disconnected from the QUIZZGAME!" << endl;
            pthread_exit(NULL);
        }

        int max1 = 0, max2 = 0, max3 = 0;
        string username1, username2, username3;

        for (int i = 0; i < client_number - 1; i++)
        {
            for (int j = 0; j < client_number; j++)
            {
                if (scores[i] < scores[j])
                {
                    swap(scores[i], scores[j]);
                    swap(users[i], users[j]);
                }
            }
        }
        bzero(question, 500);
        switch (client_number)
        {
        case 1:
        {
            max1 = scores[0];
            username1 = users[0];
            cout << "[Game Thread] First place: " << username1 << endl;
            snprintf(question, sizeof(question), "\033[1;34mFirst place:\033[1;0m %s", username1.c_str());
            max2 = -1;
            max3 = -1;
        }
        break;
        case 2:
        {
            max1 = scores[0];
            username1 = users[0];
            max2 = scores[1];
            username2 = users[1];
            cout << "[Game Thread] First place: " << username1 << endl;
            cout << "[Game Thread] Second place winner is: " << username2 << endl;
            snprintf(question, sizeof(question), "\033[1;34mFirst place:\033[1;0m %s\n\033[1;34mSecond place:\033[1;0m %s", username1.c_str(), username2.c_str());
            max3 = -1;
        }
        break;
        default:
        {
            max1 = scores[0];
            username1 = users[0];
            max2 = scores[1];
            username2 = users[1];
            max3 = scores[2];
            username3 = users[2];
            cout << "[Game Thread] First place: " << username1 << endl;
            cout << "[Game Thread] Second place winner is: " << username2 << endl;
            cout << "[Game Thread] Third place winner is: " << username3 << endl;
            snprintf(question, sizeof(question), "\033[1;34mFirst place:\033[1;0m %s\n\033[1;34mSecond place: \033[1;0m%s\n\033[1;34mThird place:\033[1;0m %s\n", username1.c_str(), username2.c_str(), username3.c_str());
        }
        }
        /* sending the podium */
        if (write(fd, question, strlen(question)) < 0)
        {
            perror("[Game Thread] Error at write() to client.\n");
        }
        bzero(response, 255);
        if (read(fd, response, sizeof(response)) < 0)
        {
            perror("[Game Thread] Error at read() from client.\n");
        }
        if (strcmp(response, "disconnect") == 0)
        {
            Remove_client(fd);
            cout << "[Game Thread] Client with fd " << fd << " disconnected from the QUIZZGAME!" << endl;
            pthread_exit(NULL);
        }
        pthread_exit(NULL);
    }
    void StartGame()
    {
        printf("[Game] The game with id: %d has started!\n", id_uniq);
        int nr1 = 0, nr2 = 0, nr3 = 0;
        int number_of_threads = client_number;
        vector<thread> threads;

        for (int i = 0; i < client_number; i++)
        {
            threads.emplace_back(&Game::handleGame, this, clients[i]);
        }

        for (int i = 0; i < number_of_threads; i++)
        {
            threads[i].join();
        }

        end_game = true;
        cout << "[Game] The game " << id_uniq << " ended." << endl;
    }
};
