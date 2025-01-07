﻿#include "app.h"
#include "player/RealTimePlayer.h"
#include "resources/render_image.h"
#include "sdp_handler.h"
#include "wifi/WFBReceiver.h"

#include <glad/gl.h>
#include <nodes/ui/menu_button.h>
#include <servers/render_server.h>

class MyRenderRect : public Flint::TextureRect {
public:
    std::shared_ptr<RealTimePlayer> player_;
    std::string playing_file_;
    bool playing_ = false;

    std::shared_ptr<Flint::VectorImage> logo_;
    std::shared_ptr<Flint::RenderImage> render_image_;

    void custom_ready() override {
        logo_ = std::make_shared<Flint::VectorImage>("openipc-logo-white.svg");
        texture = logo_;

        auto render_server = Flint::RenderServer::get_singleton();
        player_ = std::make_shared<RealTimePlayer>(render_server->device_, render_server->queue_);

        render_image_ = std::make_shared<Flint::RenderImage>(Pathfinder::Vec2I { 1920, 1080 });

        set_stretch_mode(StretchMode::KeepAspectCentered);
    }

    void custom_update(double delta) override { player_->update(delta); }

    void custom_draw() override {
        if (!playing_) {
            return;
        }
        auto render_image = (Flint::RenderImage *)texture.get();
        player_->m_yuv_renderer->render(render_image->get_texture());
    }

    void start_playing(std::string url) {
        playing_ = true;
        player_->play(url);
        texture = render_image_;
    }

    void stop_playing() {
        playing_ = false;
        player_->stop();
        texture = logo_;
    }
};

class MyControlPanel : public Flint::Panel {
    std::shared_ptr<Flint::MenuButton> dongle_menu_button_;
    std::shared_ptr<Flint::Tree> dongle_menu_;

    std::string vidPid = "0bda:8812";
    int channel = 173;
    int channelWidthMode = 0;
    std::string keyPath = "D:/Dev/Projects/fpv4win/gs.key";
    std::string codec = "AUTO";

    std::shared_ptr<Flint::Button> play_button_;

    void update_dongle_list(Flint::PopupMenu &menu) const {
        auto dongles = SdpHandler::GetDongleList();

        for (auto dongle : dongles) {
            menu.create_item(dongle);
        }
    }

    void custom_ready() override {
        auto margin_container = std::make_shared<Flint::MarginContainer>();
        margin_container->set_margin_all(8);
        margin_container->set_anchor_flag(Flint::AnchorFlag::FullRect);
        add_child(margin_container);

        auto vbox_container = std::make_shared<Flint::VBoxContainer>();
        vbox_container->set_separation(8);

        margin_container->add_child(vbox_container);

        {
            auto label = std::make_shared<Flint::Label>();
            label->set_text("RTL8812AU VID:PID");
            vbox_container->add_child(label);
        }

        {
            dongle_menu_button_ = std::make_shared<Flint::MenuButton>();
            dongle_menu_button_->set_text("Select a device");
            vbox_container->add_child(dongle_menu_button_);

            auto dongle_menu = dongle_menu_button_->get_popup_menu();

            update_dongle_list(*dongle_menu.lock());
        }

        {
            auto hbox_container = std::make_shared<Flint::HBoxContainer>();
            vbox_container->add_child(hbox_container);

            auto label = std::make_shared<Flint::Label>();
            label->set_text("Channel:");
            hbox_container->add_child(label);

            auto channel_edit = std::make_shared<Flint::TextEdit>();
            channel_edit->container_sizing.expand_h = true;
            channel_edit->container_sizing.flag_h = Flint::ContainerSizingFlag::Fill;
            channel_edit->set_text("173");
            hbox_container->add_child(channel_edit);
        }

        {
            auto hbox_container = std::make_shared<Flint::HBoxContainer>();
            vbox_container->add_child(hbox_container);

            auto label = std::make_shared<Flint::Label>();
            label->set_text("Channel Width:");
            hbox_container->add_child(label);

            auto channel_edit = std::make_shared<Flint::TextEdit>();
            channel_edit->container_sizing.expand_h = true;
            channel_edit->container_sizing.flag_h = Flint::ContainerSizingFlag::Fill;
            channel_edit->set_text("20");
            hbox_container->add_child(channel_edit);
        }

        {
            auto label = std::make_shared<Flint::Label>();
            label->set_text("Key:");
            vbox_container->add_child(label);

            auto hbox_container = std::make_shared<Flint::HBoxContainer>();
            vbox_container->add_child(hbox_container);

            auto text_edit = std::make_shared<Flint::TextEdit>();
            text_edit->set_editable(false);
            text_edit->set_text("gs.key");
            text_edit->container_sizing.expand_h = true;
            text_edit->container_sizing.flag_h = Flint::ContainerSizingFlag::Fill;
            hbox_container->add_child(text_edit);

            auto file_dialog = std::make_shared<Flint::FileDialog>();
            add_child(file_dialog);

            auto select_button = std::make_shared<Flint::Button>();
            select_button->set_text("Open");

            std::weak_ptr file_dialog_weak = file_dialog;
            std::weak_ptr text_edit_weak = text_edit;
            auto callback = [file_dialog_weak, text_edit_weak] {
                auto path = file_dialog_weak.lock()->show();
                if (path.has_value()) {
                    std::filesystem::path p(path.value());
                    text_edit_weak.lock()->set_text(p.filename().string());
                }
            };
            select_button->connect_signal("pressed", callback);
            hbox_container->add_child(select_button);
        }

        {
            play_button_ = std::make_shared<Flint::Button>();
            play_button_->set_text("Start");
            play_button_->container_sizing.expand_h = true;
            play_button_->container_sizing.flag_h = Flint::ContainerSizingFlag::Fill;

            auto callback1 = [this] {
                if (this->play_button_->get_text() == "Start") {
                    this->play_button_->set_text("Stop");
                    SdpHandler::Instance().Start(vidPid, channel, channelWidthMode, keyPath, codec);
                } else {
                    this->play_button_->set_text("Start");
                    SdpHandler::Instance().Stop();
                }
            };
            play_button_->connect_signal("pressed", callback1);
            vbox_container->add_child(play_button_);
        }
    }
};

int main() {
    Flint::App app({ 1280, 720 });
    Flint::Logger::set_level(Flint::Logger::Level::Silence);

    auto hbox_container = std::make_shared<Flint::HBoxContainer>();
    hbox_container->set_separation(8);
    hbox_container->set_anchor_flag(Flint::AnchorFlag::FullRect);
    app.get_tree_root()->add_child(hbox_container);

    auto render_rect = std::make_shared<MyRenderRect>();
    render_rect->set_custom_minimum_size({ 640, 360 });
    render_rect->container_sizing.expand_h = true;
    render_rect->container_sizing.flag_h = Flint::ContainerSizingFlag::Fill;
    hbox_container->add_child(render_rect);

    auto control_panel = std::make_shared<MyControlPanel>();
    control_panel->set_custom_minimum_size({ 280, 0 });
    control_panel->container_sizing.expand_v = true;
    control_panel->container_sizing.flag_v = Flint::ContainerSizingFlag::Fill;
    hbox_container->add_child(control_panel);

    auto render_rect_raw = render_rect.get();
    auto onRtpStream = [render_rect_raw](std::string sdp_file) {
        render_rect_raw->playing_file_ = sdp_file;
        render_rect_raw->start_playing(sdp_file);
    };
    SdpHandler::Instance().onRtpStream = onRtpStream;

    auto onWifiStop = [render_rect_raw]() { render_rect_raw->stop_playing(); };
    SdpHandler::Instance().onWifiStop = onWifiStop;

    app.main_loop();

    return EXIT_SUCCESS;
}
