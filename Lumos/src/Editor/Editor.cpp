#include "lmpch.h"
#include "Editor.h"
#include "Console.h"
#include "ImGUIConsoleSink.h"
#include "App/Application.h"
#include "Core/OS/Input.h"
#include "Core/Profiler.h"
#include "App/Engine.h"
#include "App/Scene.h"
#include "App/SceneManager.h"

#include "Maths/BoundingSphere.h"
#include "ECS/EntityManager.h"
#include "ECS/Component/Components.h"
#include "ECS/SystemManager.h"
#include "Physics/LumosPhysicsEngine/LumosPhysicsEngine.h"

#include "Graphics/GBuffer.h"
#include "Graphics/Camera/Camera.h"
#include "Graphics/RenderManager.h"
#include "Graphics/Layers/LayerStack.h"
#include "Graphics/API/GraphicsContext.h"
#include "Graphics/API/Texture.h"
#include "Graphics/API/GraphicsContext.h"

#include <imgui/imgui_internal.h>
#include <imgui/plugins/ImGuizmo.h>
#include <IconFontCppHeaders/IconsFontAwesome5.h>

namespace Lumos
{
	Editor::Editor(Application* app, u32 width, u32 height) : m_Application(app)
	{
		m_SceneViewSize = Maths::Vector2(static_cast<float>(width), static_cast<float>(height));
	}

	Editor::~Editor()
	{
	}

	void Editor::OnImGui()
	{
		DrawMenuBar();
		DrawInfoBar();

		BeginDockSpace(true);
        EndDockSpace();

		//SelectEntity();
        if(m_ShowSceneView)
            DrawSceneView();
        if(m_ShowConsole)
            DrawConsole();
        if(m_ShowHierarchy)
            DrawHierarchyWindow();
        if(m_ShowInspector)
            DrawInspectorWindow();
        if(m_ShowGraphicsInfo)
            DrawGraphicsInfoWindow();

        if(m_ShowProfiler)
        {
            Profiler::Instance()->OnImGui();
        }

        if(m_ShowImGuiDemo)
            ImGui::ShowDemoWindow(&m_ShowImGuiDemo);

	}

	void Editor::DrawMenuBar()
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Exit")) { Application::Instance()->SetAppState(AppState::Closing); }
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Edit"))
			{
				if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
				if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
				ImGui::Separator();
				if (ImGui::MenuItem("Cut", "CTRL+X")) {}
				if (ImGui::MenuItem("Copy", "CTRL+C")) {}
				if (ImGui::MenuItem("Paste", "CTRL+V")) {}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Windows"))
			{
				if (ImGui::MenuItem("Console", "", &m_ShowConsole, true)) { m_ShowConsole = true;  }
				if (ImGui::MenuItem("Hierarchy", "", &m_ShowHierarchy, true)) { m_ShowHierarchy = true; }
				if (ImGui::MenuItem("Scene", "", &m_ShowSceneView, true)) { m_ShowSceneView = true; }
				if (ImGui::MenuItem("Inspector", "", &m_ShowInspector, true)) { m_ShowInspector = true; }
				if (ImGui::MenuItem("GraphicsInfo", "", &m_ShowGraphicsInfo, true)) { m_ShowGraphicsInfo = true; }
				if (ImGui::MenuItem("ImGuiExample", "", &m_ShowImGuiDemo, true)) { m_ShowImGuiDemo = true; }
                if (ImGui::MenuItem("Profiler", "", &m_ShowProfiler, true)) { m_ShowProfiler = true;  }

				ImGui::EndMenu();
			}
            
            if (ImGui::BeginMenu("Scenes"))
            {
                auto scenes = Application::Instance()->GetSceneManager()->GetSceneNames();
                
                for(size_t i = 0; i < scenes.size(); i++)
                {
                    auto name = scenes[i];
                    if (ImGui::MenuItem(name.c_str()))
                    {
                        Application::Instance()->GetSceneManager()->SwitchScene(name);
                    }
                }
                ImGui::EndMenu();
            }

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 20.0f);
			if (ImGui::Button(ICON_FA_TIMES, ImVec2(19.0f, 19.0f)))
				Application::Instance()->SetAppState(AppState::Closing);

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::TextUnformatted("Exit");
				ImGui::EndTooltip();
			}

			ImGui::EndMainMenuBar();
		}
	}
    
    void Editor::DrawNode(Entity* node)
    {
        if (node == nullptr)
            return;
        
        bool show = true;
        
        if(m_HierarchyFilter.IsActive())
        {
            if(!m_HierarchyFilter.PassFilter((node->GetName().c_str())))
            {
                show = false;
            }
        }
        
        if(show)
        {
			bool noChildren = node->GetChildren().empty();

			ImGuiTreeNodeFlags nodeFlags = ((m_Selected == node) ? ImGuiTreeNodeFlags_Selected : 0);
              
			nodeFlags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
              
			if(noChildren)
			{
				nodeFlags |= ImGuiTreeNodeFlags_Leaf;
			}

			String icon(ICON_FA_CUBE);
			bool nodeOpen = ImGui::TreeNodeEx(("##" + node->GetUUID()).c_str(), nodeFlags, (icon + " " + node->GetName()).c_str(), 0);

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				auto ptr = node;
				ImGui::SetDragDropPayload("Drag_Entity", &ptr, sizeof(Entity**));
				ImGui::Text("Moving %s", node->GetName().c_str());
				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Drag_Entity"))
				{
					LUMOS_ASSERT(payload->DataSize == sizeof(Entity**), "Error ImGUI drag entity");
					auto entity = *reinterpret_cast<Entity**>(payload->Data);
					node->AddChild(entity);

					if (m_Selected == entity)
						m_Selected = nullptr;
				}
				ImGui::EndDragDropTarget();
			}

			if (ImGui::IsItemClicked())
				m_Selected = node;
              
			if (nodeOpen == false)
				return;
            
            for (auto child : node->GetChildren())
            {
                this->DrawNode(child);
            }
            
            ImGui::TreePop();

        }
        else
        {
            for (auto child : node->GetChildren())
            {
                this->DrawNode(child);
            }
        }
    }

	void Editor::DrawHierarchyWindow()
	{
		auto flags = ImGuiWindowFlags_NoCollapse;
		ImGui::Begin(ICON_FA_LIST_ALT" Hierarchy###hierarchy", &m_ShowHierarchy, flags);
		{
			ImGui::Indent();
            m_HierarchyFilter.Draw(ICON_FA_SEARCH);
            
			if (ImGui::TreeNode("Application"))
			{
				auto systems = Application::Instance()->GetSystemManager();

				if (ImGui::TreeNode("Systems"))
				{
					systems->OnImGui();
					ImGui::TreePop();
				}

				auto layerStack = Application::Instance()->GetLayerStack();
				if (ImGui::TreeNode("Layers"))
				{
					layerStack->OnImGui();
					ImGui::TreePop();
				}

				ImGui::NewLine();
				ImGui::Text("FPS : %5.2i", Engine::Instance()->GetFPS());
				ImGui::Text("UPS : %5.2i", Engine::Instance()->GetUPS());
				ImGui::Text("Frame Time : %5.2f ms", Engine::Instance()->GetFrametime());
				ImGui::NewLine();
				ImGui::Text("Scene : %s", m_Application->m_SceneManager->GetCurrentScene()->GetSceneName().c_str());

				bool flipImage = Graphics::GraphicsContext::GetContext()->FlipImGUITexture();

				if (ImGui::TreeNode("GBuffer"))
				{
					if (ImGui::TreeNode("Colour Texture"))
					{
						ImGui::Image(m_Application->m_RenderManager->GetGBuffer()->GetTexture(Graphics::SCREENTEX_COLOUR)->GetHandle(), ImVec2(128, 128), ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f));
                        
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::Image(m_Application->m_RenderManager->GetGBuffer()->GetTexture(Graphics::SCREENTEX_COLOUR)->GetHandle(), ImVec2(256, 256), ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f));
                            ImGui::EndTooltip();
                        }
                        
						ImGui::TreePop();
					}
					if (ImGui::TreeNode("Normal Texture"))
					{
						ImGui::Image(m_Application->m_RenderManager->GetGBuffer()->GetTexture(Graphics::SCREENTEX_NORMALS)->GetHandle(), ImVec2(128, 128), ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f));
                        
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::Image(m_Application->m_RenderManager->GetGBuffer()->GetTexture(Graphics::SCREENTEX_NORMALS)->GetHandle(), ImVec2(256, 256), ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f));
                            ImGui::EndTooltip();
                        }
                        
						ImGui::TreePop();
					}
					if (ImGui::TreeNode("PBR Texture"))
					{
						ImGui::Image(m_Application->m_RenderManager->GetGBuffer()->GetTexture(Graphics::SCREENTEX_PBR)->GetHandle(), ImVec2(128, 128), ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f));
                        
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::Image(m_Application->m_RenderManager->GetGBuffer()->GetTexture(Graphics::SCREENTEX_PBR)->GetHandle(), ImVec2(256, 256), ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f));
                            ImGui::EndTooltip();
                        }
                        
						ImGui::TreePop();
					}
					if (ImGui::TreeNode("Position Texture"))
					{
						ImGui::Image(m_Application->m_RenderManager->GetGBuffer()->GetTexture(Graphics::SCREENTEX_POSITION)->GetHandle(), ImVec2(128, 128), ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f));
                        
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::Image(m_Application->m_RenderManager->GetGBuffer()->GetTexture(Graphics::SCREENTEX_POSITION)->GetHandle(), ImVec2(256, 256), ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f));
                            ImGui::EndTooltip();
                        }
                        
						ImGui::TreePop();
					}
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Scene"))
			{
				ImGui::Indent();
                
				DrawNode(Application::Instance()->GetSceneManager()->GetCurrentScene()->GetRootEntity());
                
                ImGui::TreePop();
            }
			
			m_Application->m_SceneManager->GetCurrentScene()->OnImGui();
		}
		ImGui::End();
	}

	void Editor::DrawInspectorWindow()
	{
		auto flags = ImGuiWindowFlags_NoCollapse;
		ImGui::Begin(ICON_FA_INFO_CIRCLE" Inspector###inspector", &m_ShowInspector, flags);
        
		if (m_Selected)
        {
            m_Selected->OnImGui();
        }

		ImGui::End();
	}

	void Editor::DrawSceneView()
	{
		auto flags = ImGuiWindowFlags_NoCollapse;
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin(ICON_FA_GAMEPAD" Scene###scene",  &m_ShowSceneView, flags);
		
		ImGuizmo::SetDrawlist();
		m_SceneViewSize = Maths::Vector2(ImGui::GetWindowSize().x, ImGui::GetWindowSize().y);
		m_SceneViewPosition = Maths::Vector2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y);

		Camera* camera = m_Application->m_SceneManager->GetCurrentScene()->GetCamera();
		camera->SetAspectRatio(static_cast<float>(ImGui::GetWindowSize().x) / static_cast<float>(ImGui::GetWindowSize().y));

		auto width = static_cast<unsigned int>(ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x + 2);
		auto height = static_cast<unsigned int>(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y + 22);

		// Make pixel perfect
		width -= (width % 2 != 0) ? 1 : 0;
		height -= (height % 2 != 0) ? 1 : 0;

        bool flipImage = Graphics::GraphicsContext::GetContext()->FlipImGUITexture();

		ImGui::SetCursorPos({ 0,0 });
        
		ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, static_cast<float>(width), static_cast<float>(height));
		ImGui::Image(m_Application->m_RenderManager->GetGBuffer()->GetTexture(Graphics::SCREENTEX_OFFSCREEN0)->GetHandle(), ImVec2(static_cast<float>(width), static_cast<float>(height)), ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f));

		m_ShowGrid = camera->Is2D();

		if (m_ShowGrid)
		{
			if (camera->Is2D())
			{
				Draw2DGrid(ImGui::GetWindowDrawList(), { camera->GetPosition().GetX(), camera->GetPosition().GetY() }, ImGui::GetWindowPos(), { static_cast<float>(m_SceneViewSize.GetX()), static_cast<float>(m_SceneViewSize.GetY()) }, camera->GetScale(), 1.5f);
			}
			else
			{
				Maths::Matrix4 view = camera->GetViewMatrix();
				Maths::Matrix4 proj = camera->GetProjectionMatrix();
				Maths::Matrix4 identityMatrix;

#ifdef LUMOS_RENDER_API_VULKAN
				if (Graphics::GraphicsContext::GetRenderAPI() == Graphics::RenderAPI::VULKAN)
					proj[5] *= -1.0f;
#endif

				ImGuizmo::DrawGrid(view.values, proj.values, identityMatrix.values, m_GridSize, 1.0f);
			}
		}

		if (m_Selected)
		{
			OnImGuizmo();
		}

		ImGui::End();
	}

	void Editor::OnImGuizmo()
	{
		Maths::Matrix4 view = Application::Instance()->GetSceneManager()->GetCurrentScene()->GetCamera()->GetViewMatrix();
		Maths::Matrix4 proj = Application::Instance()->GetSceneManager()->GetCurrentScene()->GetCamera()->GetProjectionMatrix();

#ifdef LUMOS_RENDER_API_VULKAN
		if (Graphics::GraphicsContext::GetRenderAPI() == Graphics::RenderAPI::VULKAN)
			proj[5] *= -1.0f;
#endif
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetOrthographic(Application::Instance()->GetSceneManager()->GetCurrentScene()->GetCamera()->Is2D());

		if (m_Selected->GetComponent<TransformComponent>() != nullptr)
		{
			Maths::Matrix4 model = m_Selected->GetComponent<TransformComponent>()->GetTransform()->GetWorldMatrix();

			float snapAmount[3] = { m_SnapAmount  , m_SnapAmount , m_SnapAmount };
			float delta[16];
			ImGuizmo::Manipulate(view.values, proj.values, static_cast<ImGuizmo::OPERATION>(m_ImGuizmoOperation), ImGuizmo::LOCAL, model.values, delta, m_SnapQuizmo ? snapAmount : nullptr);

			if (ImGuizmo::IsUsing())
			{
				auto mat = Maths::Matrix4(delta) * m_Selected->GetComponent<TransformComponent>()->GetTransform()->GetLocalMatrix();
				m_Selected->GetComponent<TransformComponent>()->GetTransform()->SetLocalTransform(mat);
				m_Selected->GetComponent<TransformComponent>()->GetTransform()->ApplyTransform();

				auto physics2DComponent = m_Selected->GetComponent<Physics2DComponent>();

				if (physics2DComponent)
				{
					physics2DComponent->GetPhysicsObject()->SetPosition({ mat.GetPositionVector().GetX(), mat.GetPositionVector().GetY() });
				}
				else
				{
					auto physics3DComponent = m_Selected->GetComponent<Physics3DComponent>();
					if (physics3DComponent)
					{
						physics3DComponent->GetPhysicsObject()->SetPosition(mat.GetPositionVector());
						physics3DComponent->GetPhysicsObject()->SetOrientation(mat.GetRotation().ToQuaternion());
					}
					
				}
			}
		}
	}

	void Editor::BeginDockSpace(bool infoBar)
	{
        static bool p_open = true;
        static bool opt_fullscreen_persistant = true;
        static ImGuiDockNodeFlags opt_flags = ImGuiDockNodeFlags_None;
        bool opt_fullscreen = opt_fullscreen_persistant;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();

            auto pos = viewport->Pos;
            auto size = viewport->Size;
    
            if (infoBar)
            {
                const float infoBarSize = 24.0f;
                pos.y += infoBarSize;
                size.y -= infoBarSize;
            }
            
            ImGui::SetNextWindowPos(pos);
            ImGui::SetNextWindowSize(size);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }
        
        // When using ImGuiDockNodeFlags_PassthruDockspace, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
        if (opt_flags & ImGuiDockNodeFlags_DockSpace)
            window_flags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("MyDockspace", &p_open, window_flags);
        ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

		if (ImGui::DockBuilderGetNode(ImGui::GetID("MyDockspace")) == nullptr)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
			ImGui::DockBuilderRemoveNode(dockspace_id);
			ImGui::DockBuilderAddNode(dockspace_id);
			ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetIO().DisplaySize);

			ImGuiID dock_main_id   = dockspace_id;
			ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.2f, nullptr, &dock_main_id);
			ImGuiID dock_id_left   = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.2f, nullptr, &dock_main_id);
            ImGuiID dock_id_right  = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.3f, nullptr, &dock_main_id);
			ImGuiID dock_id_middle = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.8f, nullptr, &dock_main_id);

			ImGui::DockBuilderDockWindow(ICON_FA_GAMEPAD" Scene###scene", dock_id_middle);
			ImGui::DockBuilderDockWindow(ICON_FA_INFO_CIRCLE" Inspector###inspector", dock_id_right);
			ImGui::DockBuilderDockWindow(ICON_FA_LIST_ALT" Hierarchy###hierarchy", dock_id_left);
			ImGui::DockBuilderDockWindow(ICON_FA_ALIGN_LEFT" Console###console", dock_id_bottom);
            ImGui::DockBuilderDockWindow(ICON_FA_STOPWATCH" Profiler###profiler", dock_id_bottom);
			ImGui::DockBuilderDockWindow("Dear ImGui Demo", dock_id_left);
            ImGui::DockBuilderDockWindow("GraphicsInfo", dock_id_left);

			ImGui::DockBuilderFinish(dockspace_id);
		}
		
		  // Dockspace
          ImGuiIO& io = ImGui::GetIO();
          if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
          {
              ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
              ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), opt_flags);
          }
	}

	void Editor::EndDockSpace()
	{
		ImGui::End();
	}

	void Editor::SelectEntity()
	{
		bool bCheck = false;
		bCheck |= !ImGui::IsMouseClicked(0);

		bCheck |= !ImGui::IsWindowHovered
		(
			ImGuiHoveredFlags_AllowWhenBlockedByPopup |
			ImGuiHoveredFlags_AllowWhenBlockedByActiveItem
		);

		if (bCheck)
			return;

		Maths::Vector2 relativeMousePos = Input::GetInput()->GetMousePosition() - m_SceneViewPosition;

		float pointX = +2.0f * relativeMousePos.x / m_SceneViewSize.x - 1.0f;
		float pointY = -2.0f * relativeMousePos.y / m_SceneViewSize.y + 1.0f;

		Maths::Matrix4 view = m_Application->GetSceneManager()->GetCurrentScene()->GetCamera()->GetViewMatrix();
		Maths::Matrix4 proj = m_Application->GetSceneManager()->GetCurrentScene()->GetCamera()->GetProjectionMatrix();
		Maths::Matrix4 invProjView = Maths::Matrix4::Inverse(view * proj);

		Maths::Vector3 worldMousePos = invProjView * Maths::Vector3(pointX, pointY, 0.0f);

		/*m_Application->m_SceneManager->GetCurrentScene()->IterateEntities([&](Entity* entity)
		{
			auto boundingBox = entity->GetBoundingRadius();
			Maths::BoundingSphere test(entity->GetTransformComponent()->GetTransform()->GetWorldPosition(), boundingBox);
			if (test.Intersects(worldMousePos))
			{
				m_Selected = entity;
				return;
			}
		});*/
	}

	void Editor::OnInit()
	{
	}

	void Editor::OnNewScene(Scene * scene)
	{
		m_Selected = nullptr;
	}

	void Editor::DrawConsole()
	{
		Console::Instance()->OnImGuiRender(&m_ShowConsole);
	}
    
    void Editor::DrawGraphicsInfoWindow()
    {
		auto flags = ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("GraphicsInfo", &m_ShowGraphicsInfo, flags);
        {
            Graphics::GraphicsContext::GetContext()->OnImGui();
        }
        ImGui::End();
    }

	void Editor::DrawInfoBar()
	{
		auto windowFlags
			= ImGuiWindowFlags_NoDocking
			| ImGuiWindowFlags_NoBringToFrontOnFocus
			| ImGuiWindowFlags_NoNavFocus
			| ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoCollapse
			| ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoScrollbar
			//| ImGuiWindowFlags_NoBackground
			| ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoScrollWithMouse;
            //| ImGuiWindowFlags_MenuBar;

		ImGuiViewport* viewport = ImGui::GetMainViewport();

		auto pos = viewport->Pos;
		auto size = viewport->Size;

		size.y = 24.0f;
		pos.y += 20.0f;
		ImGui::SetNextWindowPos(pos);
		ImGui::SetNextWindowSize(size);

		ImGui::Begin("InfoBar", nullptr, windowFlags);
		{
            ImGui::Indent();
			bool selected = false;
			{
				selected = m_ImGuizmoOperation == ImGuizmo::TRANSLATE;
				if (selected)
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

				if (ImGui::Button(ICON_FA_ARROWS_ALT, ImVec2(19.0f, 19.0f)))
					m_ImGuizmoOperation = ImGuizmo::TRANSLATE;

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("Translate");
					ImGui::EndTooltip();
				}

				if (selected)
					ImGui::PopStyleColor();
			}

			{
				selected = m_ImGuizmoOperation == ImGuizmo::ROTATE;
				if (selected)
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

				ImGui::SameLine();
				if (ImGui::Button(ICON_FA_SYNC, ImVec2(19.0f, 19.0f)))
					m_ImGuizmoOperation = ImGuizmo::ROTATE;

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("Rotate");
					ImGui::EndTooltip();
				}

				if (selected)
					ImGui::PopStyleColor();
			}

			{
				selected = m_ImGuizmoOperation == ImGuizmo::SCALE;
				if (selected)
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

				ImGui::SameLine();
				if (ImGui::Button(ICON_FA_EXPAND_ARROWS_ALT, ImVec2(19.0f, 19.0f)))
					m_ImGuizmoOperation = ImGuizmo::SCALE;

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("Scale");
					ImGui::EndTooltip();
				}

				if (selected)
					ImGui::PopStyleColor();
			}

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x / 2.0f);

			{
				selected = m_Application->GetEditorState() == EditorState::Play;
				if (selected)
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.2f, 0.7f, 1.0f));

				if (ImGui::Button(ICON_FA_PLAY, ImVec2(19.0f, 19.0f)))
					m_Application->SetEditorState(EditorState::Play);

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("Play");
					ImGui::EndTooltip();
				}

				if (selected)
					ImGui::PopStyleColor();
			}

			ImGui::SameLine();

			{
				selected = m_Application->GetEditorState() == EditorState::Paused;
				if (selected)
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.2f, 0.7f, 1.0f));

				if (ImGui::Button(ICON_FA_PAUSE, ImVec2(19.0f, 19.0f)))
					m_Application->SetEditorState(EditorState::Paused);

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("Pause");
					ImGui::EndTooltip();
				}

				if (selected)
					ImGui::PopStyleColor();
			}

			ImGui::SameLine();

			{
				selected = m_Application->GetEditorState() == EditorState::Next;
				if (selected)
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.2f, 0.7f, 1.0f));

				if (ImGui::Button(ICON_FA_STEP_FORWARD, ImVec2(19.0f, 19.0f)))
					m_Application->SetEditorState(EditorState::Next);

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("Next");
					ImGui::EndTooltip();
				}

				if (selected)
					ImGui::PopStyleColor();
			}

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 130.0f);
			ImGui::Text("%.2f ms (%i FPS)", 1000.0f / (float)Engine::Instance()->GetFPS(), Engine::Instance()->GetFPS());

		}
		ImGui::End();
	}

	static ImVec2 operator+(const ImVec2 &a, const ImVec2 &b) {
		return ImVec2(a.x + b.x, a.y + b.y);
	}

	static ImVec2 operator-(const ImVec2 &a, const ImVec2 &b) {
		return ImVec2(a.x - b.x, a.y - b.y);
	}

	void Editor::Draw2DGrid(ImDrawList* drawList, const ImVec2& cameraPos, const ImVec2& windowPos, const ImVec2& canvasSize, const float factor, const float thickness)
	{
		ImU32 GRID_COLOR = IM_COL32(100, 100, 100, 40);
	
		static const auto graduation = ImVec2{ 10, 10 };
		float GRID_SZ = 64.0f / factor;
		const ImVec2& offset = { canvasSize.x / 2.0f - cameraPos.x * 300.0f / factor, canvasSize.y / 2.0f + cameraPos.y* 300.0f / factor };// cameraPos;

		const auto& gridColor = GRID_COLOR;
		const auto& smallGridColor = IM_COL32(100, 10, 100, 40);

		for (float x = -GRID_SZ; x < canvasSize.x + GRID_SZ; x += GRID_SZ)
		{
			auto localX = floorf(x + fmodf(offset.x, GRID_SZ));
			drawList->AddLine(ImVec2{ localX, 0.0f } + windowPos, ImVec2{ localX, canvasSize.y } +windowPos, gridColor);

			auto smallGraduation = GRID_SZ / graduation.x;

			for (int i = 1; i < graduation.x; ++i)
			{
				const auto graduation = floorf(localX + smallGraduation * i);
				drawList->AddLine(ImVec2{ graduation, 0.0f } +windowPos, ImVec2{ graduation, canvasSize.y } +windowPos, smallGridColor);
			}
		}

		for (float y = -GRID_SZ; y < canvasSize.y + GRID_SZ; y += GRID_SZ)
		{
			auto localY = floorf(y + fmodf(offset.y, GRID_SZ));
			drawList->AddLine(ImVec2{ 0.0f, localY } +windowPos, ImVec2{ canvasSize.x, localY } +windowPos, gridColor);

			auto smallGraduation = GRID_SZ / graduation.y;

			for (int i = 1; i < graduation.y; ++i)
			{
				const auto graduation = floorf(localY + smallGraduation * i);
				drawList->AddLine(ImVec2{ 0.0f, graduation } +windowPos, ImVec2{ canvasSize.x, graduation } +windowPos, smallGridColor);
			}
		}
	}
}
