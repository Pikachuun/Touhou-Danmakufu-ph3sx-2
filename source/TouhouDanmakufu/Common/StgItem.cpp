#include "source/GcLib/pch.h"

#include "StgItem.hpp"
#include "StgSystem.hpp"
#include "StgStageScript.hpp"
#include "StgPlayer.hpp"

/**********************************************************
//StgItemManager
**********************************************************/
StgItemManager::StgItemManager(StgStageController* stageController) {
	stageController_ = stageController;
	listItemData_ = new StgItemDataList();

	const std::wstring& dir = EPathProperty::GetSystemImageDirectory();

	listSpriteItem_ = new SpriteList2D();
	std::wstring pathItem = PathProperty::GetUnique(dir + L"System_Stg_Item.png");
	shared_ptr<Texture> textureItem(new Texture());
	textureItem->CreateFromFile(pathItem, false, false);
	listSpriteItem_->SetTexture(textureItem);

	listSpriteDigit_ = new SpriteList2D();
	std::wstring pathDigit = PathProperty::GetUnique(dir + L"System_Stg_Digit.png");
	shared_ptr<Texture> textureDigit(new Texture());
	textureDigit->CreateFromFile(pathDigit, false, false);
	listSpriteDigit_->SetTexture(textureDigit);

	rcDeleteClip_ = DxRect<LONG>(-64, 0, 64, 64);

	bCancelToPlayer_ = false;
	bAllItemToPlayer_ = false;
	bDefaultBonusItemEnable_ = true;

	{
		RenderShaderLibrary* shaderManager_ = ShaderManager::GetBase()->GetRenderLib();
		effectLayer_ = shaderManager_->GetRender2DShader();
		handleEffectWorld_ = effectLayer_->GetParameterBySemantic(nullptr, "WORLD");
	}
	{
		auto objectManager = stageController_->GetMainObjectManager();
		listRenderQueue_.resize(objectManager->GetRenderBucketCapacity());
		for (auto& iLayer : listRenderQueue_)
			iLayer.second.resize(32);
	}
}
StgItemManager::~StgItemManager() {
	ptr_delete(listSpriteItem_);
	ptr_delete(listSpriteDigit_);
	ptr_delete(listItemData_);
}
void StgItemManager::Work() {
	ref_unsync_ptr<StgPlayerObject> objPlayer = stageController_->GetPlayerObject();
	if (objPlayer == nullptr) return;

	float px = objPlayer->GetX();
	float py = objPlayer->GetY();
	int pr = objPlayer->GetItemIntersectionRadius() * objPlayer->GetItemIntersectionRadius();
	int pAutoItemCollectY = objPlayer->GetAutoItemCollectY();

	for (auto itr = listObj_.begin(); itr != listObj_.end();) {
		ref_unsync_ptr<StgItemObject>& obj = *itr;

		if (obj->IsDeleted()) {
			//obj->Clear();
			itr = listObj_.erase(itr);
		}
		else {
			float ix = obj->GetPositionX();
			float iy = obj->GetPositionY();

			if (objPlayer->GetState() == StgPlayerObject::STATE_NORMAL) {
				bool bMoveToPlayer = false;

				float dx = px - ix;
				float dy = py - iy;

				//if (obj->GetItemType() == StgItemObject::ITEM_SCORE && obj->GetFrameWork() > 60 * 15)
				//	obj->Intersect(nullptr, nullptr);

				int typeCollect = StgItemObject::COLLECT_PLAYER_SCOPE;
				uint64_t collectParam = 0;

				{
					int radius = dx * dx + dy * dy;
					if (obj->bIntersectEnable_ && radius <= obj->itemIntersectRadius_) {
						obj->Intersect(nullptr, nullptr);
						goto lab_next_item;
					}
					else if (radius <= pr) {
						typeCollect = StgItemObject::COLLECT_PLAYER_SCOPE;
						collectParam = (uint64_t)objPlayer->GetItemIntersectionRadius();
						bMoveToPlayer = true;
					}
				}

				if (bCancelToPlayer_ && obj->IsMoveToPlayer()) {
					obj->SetMoveToPlayer(false);
					obj->NotifyItemCancelEvent(StgItemObject::CANCEL_ALL);
				}
				else if (obj->IsPermitMoveToPlayer() && !obj->IsMoveToPlayer()) {
					if (bMoveToPlayer)
						goto lab_move_to_player;

					if (bAllItemToPlayer_) {
						typeCollect = StgItemObject::COLLECT_ALL;
						bMoveToPlayer = true;
						goto lab_move_to_player;
					}

					if (pAutoItemCollectY >= 0) {
						if (!obj->IsMoveToPlayer() && py <= pAutoItemCollectY) {
							typeCollect = StgItemObject::COLLECT_PLAYER_LINE;
							collectParam = (uint64_t)pAutoItemCollectY;
							bMoveToPlayer = true;
							goto lab_move_to_player;
						}
					}

					for (DxCircle& circle : listCircleToPlayer_) {
						float rr = circle.GetR() * circle.GetR();
						if (Math::HypotSq(ix - circle.GetX(), iy - circle.GetY()) <= rr) {
							typeCollect = StgItemObject::COLLECT_IN_CIRCLE;
							collectParam = (uint64_t)circle.GetR();
							bMoveToPlayer = true;
							goto lab_move_to_player;
						}
					}

					if (bMoveToPlayer) {
lab_move_to_player:
						obj->SetMoveToPlayer(true);
						obj->NotifyItemCollectEvent(typeCollect, collectParam);
					}
				}
			}
			else if (obj->IsMoveToPlayer()) {
				obj->SetMoveToPlayer(false);
				obj->NotifyItemCancelEvent(StgItemObject::CANCEL_PLAYER_DOWN);
			}

lab_next_item:
			++itr;
		}
	}

	listCircleToPlayer_.clear();

	bAllItemToPlayer_ = false;
	bCancelToPlayer_ = false;
}
void StgItemManager::Render(int targetPriority) {
	if (targetPriority < 0 || targetPriority >= listRenderQueue_.size()) return;

	auto& renderQueueLayer = listRenderQueue_[targetPriority];
	if (renderQueueLayer.first == 0) return;

	DirectGraphics* graphics = DirectGraphics::GetBase();
	IDirect3DDevice9* device = graphics->GetDevice();

	graphics->SetZBufferEnable(false);
	graphics->SetZWriteEnable(false);
	graphics->SetCullingMode(D3DCULL_NONE);
	graphics->SetLightingEnable(false);
	graphics->SetTextureFilter(D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_NONE);

	DWORD bEnableFog = FALSE;
	device->GetRenderState(D3DRS_FOGENABLE, &bEnableFog);
	if (bEnableFog)
		graphics->SetFogEnable(false);

	ref_count_ptr<DxCamera> camera3D = graphics->GetCamera();
	ref_count_ptr<DxCamera2D> camera2D = graphics->GetCamera2D();

	{
		D3DXMATRIX matProj;
		D3DXMatrixMultiply(&matProj, &camera2D->GetMatrix(), &graphics->GetViewPortMatrix());
		effectLayer_->SetMatrix(handleEffectWorld_, &matProj);
	}

	for (size_t i = 0; i < renderQueueLayer.first; ++i) {
		renderQueueLayer.second[i]->RenderOnItemManager();
	}

	RenderShaderLibrary* shaderManager = ShaderManager::GetBase()->GetRenderLib();
	VertexBufferManager* bufferManager = VertexBufferManager::GetBase();

	device->SetFVF(VERTEX_TLX::fvf);

	size_t countBlendType = StgItemDataList::RENDER_TYPE_COUNT;
	BlendMode blendMode[] = { 
		MODE_BLEND_ADD_ARGB, 
		MODE_BLEND_ADD_RGB, 
		MODE_BLEND_ALPHA 
	};

	{
		graphics->SetBlendMode(MODE_BLEND_ADD_ARGB);
		listSpriteDigit_->Render();
		listSpriteDigit_->ClearVertexCount();
		graphics->SetBlendMode(MODE_BLEND_ALPHA);
		listSpriteItem_->Render();
		listSpriteItem_->ClearVertexCount();

		device->SetVertexDeclaration(shaderManager->GetVertexDeclarationTLX());
		device->SetStreamSource(0, bufferManager->GetGrowableVertexBuffer()->GetBuffer(), 0, sizeof(VERTEX_TLX));

		effectLayer_->SetTechnique("Render");

		UINT cPass = 1U;
		effectLayer_->Begin(&cPass, 0);
		if (cPass >= 1) {
			for (size_t iBlend = 0; iBlend < countBlendType; iBlend++) {
				bool hasPolygon = false;
				std::vector<StgItemRenderer*>& listRenderer = listItemData_->GetRendererList(blendMode[iBlend] - 1);

				for (auto itr = listRenderer.begin(); itr != listRenderer.end() && !hasPolygon; ++itr)
					hasPolygon = (*itr)->countRenderVertex_ >= 3U;
				if (!hasPolygon) continue;

				graphics->SetBlendMode(blendMode[iBlend]);

				effectLayer_->BeginPass(0);
				for (auto itrRender = listRenderer.begin(); itrRender != listRenderer.end(); ++itrRender)
					(*itrRender)->Render(this);
				effectLayer_->EndPass();
			}
		}
		effectLayer_->End();

		device->SetVertexDeclaration(nullptr);
	}

	if (bEnableFog)
		graphics->SetFogEnable(true);
}
void StgItemManager::LoadRenderQueue() {
	for (auto& iLayer : listRenderQueue_)
		iLayer.first = 0;

	for (ref_unsync_ptr<StgItemObject>& obj : listObj_) {
		if (obj->IsDeleted() || !obj->IsActive()) continue;
		auto& iQueue = listRenderQueue_[obj->GetRenderPriorityI()];
		while (iQueue.first >= iQueue.second.size())
			iQueue.second.resize(iQueue.second.size() * 2);
		iQueue.second[(iQueue.first)++] = obj.get();
	}
}

bool StgItemManager::LoadItemData(const std::wstring& path, bool bReload) {
	return listItemData_->AddItemDataList(path, bReload);
}
ref_unsync_ptr<StgItemObject> StgItemManager::CreateItem(int type) {
	ref_unsync_ptr<StgItemObject> res;
	switch (type) {
	case StgItemObject::ITEM_1UP:
	case StgItemObject::ITEM_1UP_S:
		res = new StgItemObject_1UP(stageController_);
		break;
	case StgItemObject::ITEM_SPELL:
	case StgItemObject::ITEM_SPELL_S:
		res = new StgItemObject_Bomb(stageController_);
		break;
	case StgItemObject::ITEM_POWER:
	case StgItemObject::ITEM_POWER_S:
		res = new StgItemObject_Power(stageController_);
		break;
	case StgItemObject::ITEM_POINT:
	case StgItemObject::ITEM_POINT_S:
		res = new StgItemObject_Point(stageController_);
		break;
	case StgItemObject::ITEM_USER:
		res = new StgItemObject_User(stageController_);
		break;
	}
	res->SetItemType(type);

	return res;
}
void StgItemManager::CollectItemsAll() {
	bAllItemToPlayer_ = true;
}
void StgItemManager::CollectItemsInCircle(const DxCircle& circle) {
	listCircleToPlayer_.push_back(circle);
}
void StgItemManager::CancelCollectItems() {
	bCancelToPlayer_ = true;
}

std::vector<int> StgItemManager::GetItemIdInCircle(int cx, int cy, int radius, int* itemType) {
	int rr = radius * radius;

	std::vector<int> res;
	for (ref_unsync_ptr<StgItemObject>& obj : listObj_) {
		if (obj->IsDeleted()) continue;
		if (itemType != nullptr && (*itemType != obj->GetItemType())) continue;

		if (Math::HypotSq<int>(cx - obj->GetPositionX(), cy - obj->GetPositionY()) <= rr)
			res.push_back(obj->GetObjectID());
	}

	return res;
}

/**********************************************************
//StgItemDataList
**********************************************************/
StgItemDataList::StgItemDataList() {
	listRenderer_.resize(RENDER_TYPE_COUNT);
}
StgItemDataList::~StgItemDataList() {
	for (std::vector<StgItemRenderer*>& renderList : listRenderer_) {
		for (StgItemRenderer*& renderer : renderList)
			ptr_delete(renderer);
		renderList.clear();
	}
	listRenderer_.clear();

	for (StgItemData*& itemData : listData_)
		ptr_delete(itemData);
	listData_.clear();
}
bool StgItemDataList::AddItemDataList(const std::wstring& path, bool bReload) {
	if (!bReload && listReadPath_.find(path) != listReadPath_.end()) return true;

	shared_ptr<FileReader> reader = FileManager::GetBase()->GetFileReader(path);
	if (reader == nullptr || !reader->Open()) 
		throw gstd::wexception(L"AddItemDataList: " + ErrorUtility::GetFileNotFoundErrorMessage(path, true));
	std::string source = reader->ReadAllString();

	bool res = false;
	Scanner scanner(source);
	try {
		std::vector<StgItemData*> listData;

		std::wstring pathImage = L"";
		while (scanner.HasNext()) {
			Token& tok = scanner.Next();
			if (tok.GetType() == Token::Type::TK_EOF)
				break;
			else if (tok.GetType() == Token::Type::TK_ID) {
				std::wstring element = tok.GetElement();
				if (element == L"ItemData") {
					_ScanItem(listData, scanner);
				}
				else if (element == L"item_image") {
					scanner.CheckType(scanner.Next(), Token::Type::TK_EQUAL);
					pathImage = scanner.Next().GetString();
				}

				if (scanner.HasNext())
					tok = scanner.Next();
			}
		}

		//テクスチャ読み込み
		if (pathImage.size() == 0) throw gstd::wexception("Item texture must be set.");
		std::wstring dir = PathProperty::GetFileDirectory(path);
		pathImage = StringUtility::Replace(pathImage, L"./", dir);

		shared_ptr<Texture> texture(new Texture());
		bool bTexture = texture->CreateFromFile(PathProperty::GetUnique(pathImage), false, false);
		if (!bTexture) throw gstd::wexception("The specified item texture cannot be found.");

		int textureIndex = -1;
		for (int iTexture = 0; iTexture < listTexture_.size(); iTexture++) {
			shared_ptr<Texture> tSearch = listTexture_[iTexture];
			if (tSearch->GetName() == texture->GetName()) {
				textureIndex = iTexture;
				break;
			}
		}
		if (textureIndex < 0) {
			textureIndex = listTexture_.size();
			listTexture_.push_back(texture);
			for (size_t iRender = 0; iRender < listRenderer_.size(); iRender++) {
				StgItemRenderer* render = new StgItemRenderer();
				render->SetTexture(texture);
				listRenderer_[iRender].push_back(render);
			}
		}

		if (listData_.size() < listData.size())
			listData_.resize(listData.size());
		for (size_t iData = 0; iData < listData.size(); iData++) {
			StgItemData* data = listData[iData];
			if (data == nullptr) continue;

			data->indexTexture_ = textureIndex;
			{
				shared_ptr<Texture>& texture = listTexture_[data->indexTexture_];
				data->textureSize_.x = texture->GetWidth();
				data->textureSize_.y = texture->GetHeight();
			}

			listData_[iData] = data;
		}

		listReadPath_.insert(path);
		Logger::WriteTop(StringUtility::Format(L"Loaded item data: %s", path.c_str()));
		res = true;
	}
	catch (gstd::wexception& e) {
		std::wstring log = StringUtility::Format(L"Failed to load item data: [Line=%d] (%s)", scanner.GetCurrentLine(), e.what());
		Logger::WriteTop(log);
		res = false;
	}
	catch (...) {
		std::wstring log = StringUtility::Format(L"Failed to load item data: [Line=%d] (Unknown error.)", scanner.GetCurrentLine());
		Logger::WriteTop(log);
		res = false;
	}

	return res;
}
void StgItemDataList::_ScanItem(std::vector<StgItemData*>& listData, Scanner& scanner) {
	Token& tok = scanner.Next();
	if (tok.GetType() == Token::Type::TK_NEWLINE) tok = scanner.Next();
	scanner.CheckType(tok, Token::Type::TK_OPENC);

	StgItemData* data = new StgItemData(this);
	int id = -1;
	int typeItem = -1;

	while (true) {
		tok = scanner.Next();
		if (tok.GetType() == Token::Type::TK_CLOSEC) {
			break;
		}
		else if (tok.GetType() == Token::Type::TK_ID) {
			std::wstring element = tok.GetElement();

			if (element == L"id") {
				scanner.CheckType(scanner.Next(), Token::Type::TK_EQUAL);
				id = scanner.Next().GetInteger();
			}
			else if (element == L"type") {
				scanner.CheckType(scanner.Next(), Token::Type::TK_EQUAL);
				typeItem = scanner.Next().GetInteger();
			}
			else if (element == L"rect") {
				std::vector<std::wstring> list = _GetArgumentList(scanner);

				StgItemData::AnimationData anime;

				DxRect<int> rect(
					StringUtility::ToInteger(list[0]),
					StringUtility::ToInteger(list[1]),
					StringUtility::ToInteger(list[2]),
					StringUtility::ToInteger(list[3]));
				anime.rcSrc_ = rect;

				LONG width = rect.right - rect.left;
				LONG height = rect.bottom - rect.top;
				DxRect<int> rcDest(-width / 2, -height / 2, width / 2, height / 2);
				if (width % 2 == 1) rcDest.right++;
				if (height % 2 == 1) rcDest.bottom++;
				anime.rcDest_ = rcDest;

				data->listAnime_.resize(1);
				data->listAnime_[0] = anime;
				data->totalAnimeFrame_ = 1;
			}
			else if (element == L"out") {
				std::vector<std::wstring> list = _GetArgumentList(scanner);

				DxRect<int> rect(
					StringUtility::ToInteger(list[0]),
					StringUtility::ToInteger(list[1]),
					StringUtility::ToInteger(list[2]),
					StringUtility::ToInteger(list[3]));
				data->rcOutSrc_ = rect;

				LONG width = rect.right - rect.left;
				LONG height = rect.bottom - rect.top;
				DxRect<int> rcDest(-width / 2, -height / 2, width / 2, height / 2);
				if (width % 2 == 1) rcDest.right++;
				if (height % 2 == 1) rcDest.bottom++;
				data->rcOutDest_ = rcDest;
			}
			else if (element == L"render") {
				scanner.CheckType(scanner.Next(), Token::Type::TK_EQUAL);
				std::wstring render = scanner.Next().GetElement();
				if (render == L"ADD" || render == L"ADD_RGB")
					data->typeRender_ = MODE_BLEND_ADD_RGB;
				else if (render == L"ADD_ARGB")
					data->typeRender_ = MODE_BLEND_ADD_ARGB;
			}
			else if (element == L"alpha") {
				scanner.CheckType(scanner.Next(), Token::Type::TK_EQUAL);
				data->alpha_ = scanner.Next().GetInteger();
			}
			else if (element == L"AnimationData") {
				_ScanAnimation(data, scanner);
			}
		}
	}

	if (id >= 0) {
		if (listData.size() <= id)
			listData.resize(id + 1);

		if (typeItem < 0)
			typeItem = id;
		data->typeItem_ = typeItem;

		listData[id] = data;
	}
}
void StgItemDataList::_ScanAnimation(StgItemData* itemData, Scanner& scanner) {
	Token& tok = scanner.Next();
	if (tok.GetType() == Token::Type::TK_NEWLINE) tok = scanner.Next();
	scanner.CheckType(tok, Token::Type::TK_OPENC);

	while (true) {
		tok = scanner.Next();
		if (tok.GetType() == Token::Type::TK_CLOSEC) {
			break;
		}
		else if (tok.GetType() == Token::Type::TK_ID) {
			std::wstring element = tok.GetElement();

			if (element == L"animation_data") {
				std::vector<std::wstring> list = _GetArgumentList(scanner);
				if (list.size() == 5) {
					StgItemData::AnimationData anime;
					int frame = StringUtility::ToInteger(list[0]);
					DxRect<int> rcSrc(
						StringUtility::ToInteger(list[1]),
						StringUtility::ToInteger(list[2]),
						StringUtility::ToInteger(list[3]),
						StringUtility::ToInteger(list[4]));

					LONG width = rcSrc.right - rcSrc.left;
					LONG height = rcSrc.bottom - rcSrc.top;
					DxRect<int> rcDest(-width / 2, -height / 2, width / 2, height / 2);
					if (width % 2 == 1) rcDest.right++;
					if (height % 2 == 1) rcDest.bottom++;

					anime.frame_ = frame;
					anime.rcSrc_ = rcSrc;
					anime.rcDest_ = rcDest;

					itemData->listAnime_.push_back(anime);
					itemData->totalAnimeFrame_ += frame;
				}
			}
		}
	}
}
std::vector<std::wstring> StgItemDataList::_GetArgumentList(Scanner& scanner) {
	std::vector<std::wstring> res;
	scanner.CheckType(scanner.Next(), Token::Type::TK_EQUAL);

	Token& tok = scanner.Next();

	if (tok.GetType() == Token::Type::TK_OPENP) {
		while (true) {
			tok = scanner.Next();
			Token::Type type = tok.GetType();
			if (type == Token::Type::TK_CLOSEP) break;
			else if (type != Token::Type::TK_COMMA) {
				std::wstring str = tok.GetElement();
				res.push_back(str);
			}
		}
	}
	else {
		res.push_back(tok.GetElement());
	}
	return res;
}

//StgItemData
StgItemData::StgItemData(StgItemDataList* listItemData) {
	listItemData_ = listItemData;
	typeRender_ = MODE_BLEND_ALPHA;
	alpha_ = 255;
	totalAnimeFrame_ = 0;
}
StgItemData::~StgItemData() {}
StgItemData::AnimationData* StgItemData::GetData(int frame) {
	if (totalAnimeFrame_ <= 1)
		return &listAnime_[0];

	frame = frame % totalAnimeFrame_;
	int total = 0;

	for (auto itr = listAnime_.begin(); itr != listAnime_.end(); ++itr) {
		total += itr->frame_;
		if (total >= frame)
			return &(*itr);
	}
	return &listAnime_[0];
}
StgItemRenderer* StgItemData::GetRenderer(BlendMode type) {
	if (type < MODE_BLEND_ALPHA || type > MODE_BLEND_ADD_ARGB)
		return listItemData_->GetRenderer(indexTexture_, 0);
	return listItemData_->GetRenderer(indexTexture_, type - 1);
}

/**********************************************************
//StgItemRenderer
**********************************************************/
StgItemRenderer::StgItemRenderer() {
	countRenderVertex_ = 0;
	countMaxVertex_ = 256 * 256;
	SetVertexCount(countMaxVertex_);
}
StgItemRenderer::~StgItemRenderer() {

}
void StgItemRenderer::Render(StgItemManager* manager) {
	if (countRenderVertex_ < 3) return;

	DirectGraphics* graphics = DirectGraphics::GetBase();
	IDirect3DDevice9* device = graphics->GetDevice();
	IDirect3DTexture9* pTexture = texture_ ? texture_->GetD3DTexture() : nullptr;
	device->SetTexture(0, pTexture);

	VertexBufferManager* bufferManager = VertexBufferManager::GetBase();

	GrowableVertexBuffer* vertexBuffer = bufferManager->GetGrowableVertexBuffer();
	vertexBuffer->Expand(countMaxVertex_);

	{
		BufferLockParameter lockParam = BufferLockParameter(D3DLOCK_DISCARD);

		lockParam.SetSource(vertex_, countRenderVertex_, sizeof(VERTEX_TLX));
		vertexBuffer->UpdateBuffer(&lockParam);
	}

	device->SetStreamSource(0, vertexBuffer->GetBuffer(), 0, sizeof(VERTEX_TLX));

	device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, countRenderVertex_ / 3U);

	countRenderVertex_ = 0;
}
void StgItemRenderer::AddVertex(VERTEX_TLX& vertex) {
	if (countRenderVertex_ == countMaxVertex_ - 1) {
		countMaxVertex_ *= 2;
		SetVertexCount(countMaxVertex_);
	}

	SetVertex(countRenderVertex_, vertex);
	++countRenderVertex_;
}

/**********************************************************
//StgItemObject
**********************************************************/
StgItemObject::StgItemObject(StgStageController* stageController) : StgMoveObject(stageController) {
	stageController_ = stageController;
	typeObject_ = TypeObject::Item;

	pattern_ = new StgMovePattern_Item(this);
	color_ = D3DCOLOR_ARGB(255, 255, 255, 255);

	typeItem_ = INT_MIN;

	frameWork_ = 0;

	score_ = 0;

	bMoveToPlayer_ = false;
	bPermitMoveToPlayer_ = true;

	bDefaultScoreText_ = true;

	bAutoDelete_ = true;
	bIntersectEnable_ = true;
	itemIntersectRadius_ = 16 * 16;

	bDefaultCollectionMove_ = true;

	int priItemI = stageController_->GetStageInformation()->GetItemObjectPriority();
	SetRenderPriorityI(priItemI);
}
void StgItemObject::Work() {
	if (bEnableMovement_) {
		bool bNullMovePattern = dynamic_cast<StgMovePattern_Item*>(GetPattern().get()) == nullptr;
		if (bNullMovePattern && bDefaultCollectionMove_ && IsMoveToPlayer()) {
			float speed = 8;
			ref_unsync_ptr<StgPlayerObject> objPlayer = stageController_->GetPlayerObject();
			if (objPlayer) {
				float angle = atan2f(objPlayer->GetY() - GetPositionY(), objPlayer->GetX() - GetPositionX());
				float angDirection = angle;
				SetSpeed(speed);
				SetDirectionAngle(angDirection);
			}
		}
	}
	StgMoveObject::_Move();
	SetX(posX_);
	SetY(posY_);

	_DeleteInAutoClip();

	++frameWork_;
}
void StgItemObject::RenderOnItemManager() {
	StgItemManager* itemManager = stageController_->GetItemManager();
	SpriteList2D* renderer = typeItem_ == ITEM_SCORE ?
		itemManager->GetDigitRenderer() : itemManager->GetItemRenderer();

	if (typeItem_ != ITEM_SCORE) {
		float scale = 1.0f;
		switch (typeItem_) {
		case ITEM_1UP:
		case ITEM_SPELL:
		case ITEM_POWER:
		case ITEM_POINT:
			scale = 1.0f;
			break;
		case ITEM_1UP_S:
		case ITEM_SPELL_S:
		case ITEM_POWER_S:
		case ITEM_POINT_S:
		case ITEM_BONUS:
			scale = 0.75f;
			break;
		}

		DxRect<double> rcSrc;
		switch (typeItem_) {
		case ITEM_1UP:
		case ITEM_1UP_S:
			rcSrc.Set(1, 1, 16, 16);
			break;
		case ITEM_SPELL:
		case ITEM_SPELL_S:
			rcSrc.Set(20, 1, 35, 16);
			break;
		case ITEM_POWER:
		case ITEM_POWER_S:
			rcSrc.Set(40, 1, 55, 16);
			break;
		case ITEM_POINT:
		case ITEM_POINT_S:
			rcSrc.Set(1, 20, 16, 35);
			break;
		case ITEM_BONUS:
			rcSrc.Set(20, 20, 35, 35);
			break;
		}

		//上にはみ出している
		float posY = posY_;
		D3DCOLOR color = D3DCOLOR_ARGB(255, 255, 255, 255);
		if (posY_ <= 0) {
			D3DCOLOR colorOver = D3DCOLOR_ARGB(255, 255, 255, 255);
			switch (typeItem_) {
			case ITEM_1UP:
			case ITEM_1UP_S:
				colorOver = D3DCOLOR_ARGB(255, 236, 0, 236);
				break;
			case ITEM_SPELL:
			case ITEM_SPELL_S:
				colorOver = D3DCOLOR_ARGB(255, 0, 160, 0);
				break;
			case ITEM_POWER:
			case ITEM_POWER_S:
				colorOver = D3DCOLOR_ARGB(255, 209, 0, 0);
				break;
			case ITEM_POINT:
			case ITEM_POINT_S:
				colorOver = D3DCOLOR_ARGB(255, 0, 0, 160);
				break;
			}
			if (color != colorOver) {
				rcSrc.Set(113, 1, 126, 10);
				posY = 6;
			}
			color = colorOver;
		}

		renderer->SetColor(color);
		renderer->SetPosition(posX_, posY, 0);
		renderer->SetScaleXYZ(scale, scale, scale);
		renderer->SetSourceRect(rcSrc);
		renderer->SetDestinationCenter();
		renderer->AddVertex();
	}
	else {
		renderer->SetScaleXYZ(1.0, 1.0, 1.0);
		renderer->SetColor(color_);
		renderer->SetPosition(0, 0, 0);

		int fontSize = 14;
		int64_t score = score_;
		std::vector<int> listNum;
		while (true) {
			int tnum = score % 10;
			score /= 10;
			listNum.push_back(tnum);
			if (score == 0) break;
		}
		for (int iNum = listNum.size() - 1; iNum >= 0; iNum--) {
			DxRect<double> rcSrc(listNum[iNum] * 36, 0, 
				(listNum[iNum] + 1) * 36 - 1, 31);
			DxRect<double> rcDest(posX_ + (listNum.size() - 1 - iNum) * fontSize / 2, posY_,
				posX_ + (listNum.size() - iNum)*fontSize / 2, posY_ + fontSize);
			renderer->SetSourceRect(rcSrc);
			renderer->SetDestinationRect(rcDest);
			renderer->AddVertex();
		}
	}
}
void StgItemObject::_DeleteInAutoClip() {
	if (!bAutoDelete_) return;
	DirectGraphics* graphics = DirectGraphics::GetBase();

	DxRect<LONG>* const rcStgFrame = stageController_->GetStageInformation()->GetStgFrameRect();
	DxRect<LONG>* const rcClipBase = stageController_->GetItemManager()->GetItemDeleteClip();

	bool bDelete = ((LONG)posX_ < rcClipBase->left) || ((LONG)posX_ > rcStgFrame->GetWidth() + rcClipBase->right)
		|| ((LONG)posY_ > rcStgFrame->GetHeight() + rcClipBase->bottom);
	if (!bDelete) return;

	stageController_->GetMainObjectManager()->DeleteObject(this);
}
void StgItemObject::_CreateScoreItem() {
	auto objectManager = stageController_->GetMainObjectManager();
	StgItemManager* itemManager = stageController_->GetItemManager();

	if (itemManager->GetItemCount() < StgItemManager::ITEM_MAX) {
		ref_unsync_ptr<StgItemObject_Score> obj = new StgItemObject_Score(stageController_);

		obj->SetX(posX_);
		obj->SetY(posY_);
		obj->SetScore(score_);

		objectManager->AddObject(obj);
		itemManager->AddItem(obj);
	}
}
void StgItemObject::_NotifyEventToPlayerScript(gstd::value* listValue, size_t count) {
	ref_unsync_ptr<StgPlayerObject> player = stageController_->GetPlayerObject();
	if (player == nullptr) return;

	if (StgStagePlayerScript* scriptPlayer = player->GetPlayerScript()) {
		scriptPlayer->RequestEvent(StgStageItemScript::EV_GET_ITEM, listValue, count);
	}
}
void StgItemObject::_NotifyEventToItemScript(gstd::value* listValue, size_t count) {
	auto stageScriptManager = stageController_->GetScriptManager();

	if (shared_ptr<ManagedScript> scriptItem = stageScriptManager->GetItemScript()) {
		scriptItem->RequestEvent(StgStageItemScript::EV_GET_ITEM, listValue, count);
	}
}
void StgItemObject::SetAlpha(int alpha) {
	ColorAccess::ClampColor(alpha);
	color_ = (color_ & 0x00ffffff) | ((byte)alpha << 24);
}
void StgItemObject::SetColor(int r, int g, int b) {
	__m128i c = Vectorize::Set(color_ >> 24, r, g, b);
	color_ = ColorAccess::ToD3DCOLOR(ColorAccess::ClampColorPacked(c));
}
void StgItemObject::SetToPosition(D3DXVECTOR2& pos) {
	if (auto move = ref_unsync_ptr<StgMovePattern_Item>::Cast(pattern_))
		move->SetToPosition(pos);
}
int StgItemObject::GetMoveType() {
	int res = StgMovePattern_Item::MOVE_NONE;
	if (auto move = ref_unsync_ptr<StgMovePattern_Item>::Cast(pattern_))
		res = move->GetItemMoveType();
	return res;
}
void StgItemObject::SetMoveType(int type) {
	if (auto move = ref_unsync_ptr<StgMovePattern_Item>::Cast(pattern_))
		move->SetItemMoveType(type);
}

void StgItemObject::NotifyItemCollectEvent(int type, uint64_t eventParam) {
	auto stageScriptManager = stageController_->GetScriptManager();
	if (shared_ptr<ManagedScript> scriptItem = stageScriptManager->GetItemScript()) {
		gstd::value eventArg[4];
		eventArg[0] = DxScript::CreateIntValue(idObject_);
		eventArg[1] = DxScript::CreateIntValue(typeItem_);
		eventArg[2] = DxScript::CreateIntValue(type);
		eventArg[3] = DxScript::CreateRealValue(eventParam);

		scriptItem->RequestEvent(StgStageItemScript::EV_COLLECT_ITEM, eventArg, 4U);
	}
}
void StgItemObject::NotifyItemCancelEvent(int type) {
	auto stageScriptManager = stageController_->GetScriptManager();
	if (shared_ptr<ManagedScript> scriptItem = stageScriptManager->GetItemScript()) {
		gstd::value eventArg[4];
		eventArg[0] = DxScript::CreateIntValue(idObject_);
		eventArg[1] = DxScript::CreateIntValue(typeItem_);
		eventArg[2] = DxScript::CreateIntValue(type);

		scriptItem->RequestEvent(StgStageItemScript::EV_CANCEL_ITEM, eventArg, 3U);
	}
}

//StgItemObject_1UP
StgItemObject_1UP::StgItemObject_1UP(StgStageController* stageController) : StgItemObject(stageController) {
	typeItem_ = ITEM_1UP;

	if (auto move = ref_unsync_ptr<StgMovePattern_Item>::Cast(pattern_))
		move->SetItemMoveType(StgMovePattern_Item::MOVE_TOPOSITION_A);
}
void StgItemObject_1UP::Intersect(StgIntersectionTarget* ownTarget, StgIntersectionTarget* otherTarget) {
	gstd::value listValue[2] = { 
		DxScript::CreateIntValue(typeItem_), 
		DxScript::CreateIntValue(idObject_)
	};
	_NotifyEventToPlayerScript(listValue, 2);
	_NotifyEventToItemScript(listValue, 2);

	auto objectManager = stageController_->GetMainObjectManager();
	objectManager->DeleteObject(this);
}

//StgItemObject_Bomb
StgItemObject_Bomb::StgItemObject_Bomb(StgStageController* stageController) : StgItemObject(stageController) {
	typeItem_ = ITEM_SPELL;
	if (auto move = ref_unsync_ptr<StgMovePattern_Item>::Cast(pattern_))
		move->SetItemMoveType(StgMovePattern_Item::MOVE_TOPOSITION_A);
}
void StgItemObject_Bomb::Intersect(StgIntersectionTarget* ownTarget, StgIntersectionTarget* otherTarget) {
	gstd::value listValue[2] = {
		DxScript::CreateIntValue(typeItem_),
		DxScript::CreateIntValue(idObject_)
	};
	_NotifyEventToPlayerScript(listValue, 2);
	_NotifyEventToItemScript(listValue, 2);

	auto objectManager = stageController_->GetMainObjectManager();
	objectManager->DeleteObject(this);
}

//StgItemObject_Power
StgItemObject_Power::StgItemObject_Power(StgStageController* stageController) : StgItemObject(stageController) {
	typeItem_ = ITEM_POWER;

	if (auto move = ref_unsync_ptr<StgMovePattern_Item>::Cast(pattern_))
		move->SetItemMoveType(StgMovePattern_Item::MOVE_TOPOSITION_A);

	score_ = 10;
}
void StgItemObject_Power::Intersect(StgIntersectionTarget* ownTarget, StgIntersectionTarget* otherTarget) {
	if (bDefaultScoreText_)
		_CreateScoreItem();
	stageController_->GetStageInformation()->AddScore(score_);

	gstd::value listValue[2] = {
		DxScript::CreateIntValue(typeItem_),
		DxScript::CreateIntValue(idObject_)
	};
	_NotifyEventToPlayerScript(listValue, 2);
	_NotifyEventToItemScript(listValue, 2);

	auto objectManager = stageController_->GetMainObjectManager();
	objectManager->DeleteObject(this);
}

//StgItemObject_Point
StgItemObject_Point::StgItemObject_Point(StgStageController* stageController) : StgItemObject(stageController) {
	typeItem_ = ITEM_POINT;

	if (auto move = ref_unsync_ptr<StgMovePattern_Item>::Cast(pattern_))
		move->SetItemMoveType(StgMovePattern_Item::MOVE_TOPOSITION_A);
}
void StgItemObject_Point::Intersect(StgIntersectionTarget* ownTarget, StgIntersectionTarget* otherTarget) {
	if (bDefaultScoreText_)
		_CreateScoreItem();
	stageController_->GetStageInformation()->AddScore(score_);

	gstd::value listValue[2] = {
		DxScript::CreateIntValue(typeItem_),
		DxScript::CreateIntValue(idObject_)
	};
	_NotifyEventToPlayerScript(listValue, 2);
	_NotifyEventToItemScript(listValue, 2);

	auto objectManager = stageController_->GetMainObjectManager();
	objectManager->DeleteObject(this);
}

//StgItemObject_Bonus
StgItemObject_Bonus::StgItemObject_Bonus(StgStageController* stageController) : StgItemObject(stageController) {
	typeItem_ = ITEM_BONUS;
	if (auto move = ref_unsync_ptr<StgMovePattern_Item>::Cast(pattern_))
		move->SetItemMoveType(StgMovePattern_Item::MOVE_TOPLAYER);

	int graze = stageController->GetStageInformation()->GetGraze();
	score_ = (int)(graze / 40) * 10 + 300;
}
void StgItemObject_Bonus::Work() {
	StgItemObject::Work();

	ref_unsync_ptr<StgPlayerObject> objPlayer = stageController_->GetPlayerObject();
	if (objPlayer != nullptr && objPlayer->GetState() != StgPlayerObject::STATE_NORMAL) {
		_CreateScoreItem();
		stageController_->GetStageInformation()->AddScore(score_);

		auto objectManager = stageController_->GetMainObjectManager();
		objectManager->DeleteObject(this);
	}
}
void StgItemObject_Bonus::Intersect(StgIntersectionTarget* ownTarget, StgIntersectionTarget* otherTarget) {
	_CreateScoreItem();
	stageController_->GetStageInformation()->AddScore(score_);

	auto objectManager = stageController_->GetMainObjectManager();
	objectManager->DeleteObject(this);
}

//StgItemObject_Score
StgItemObject_Score::StgItemObject_Score(StgStageController* stageController) : StgItemObject(stageController) {
	typeItem_ = ITEM_SCORE;

	if (auto move = ref_unsync_ptr<StgMovePattern_Item>::Cast(pattern_))
		move->SetItemMoveType(StgMovePattern_Item::MOVE_SCORE);

	bPermitMoveToPlayer_ = false;

	frameDelete_ = 0;
}
void StgItemObject_Score::Work() {
	StgItemObject::Work();
	int alpha = 255 - frameDelete_ * 8;
	color_ = D3DCOLOR_ARGB(alpha, alpha, alpha, alpha);

	if (frameDelete_ > 30) {
		stageController_->GetMainObjectManager()->DeleteObject(this);
		return;
	}
	++frameDelete_;
}
void StgItemObject_Score::Intersect(StgIntersectionTarget* ownTarget, StgIntersectionTarget* otherTarget) { }

//StgItemObject_User
StgItemObject_User::StgItemObject_User(StgStageController* stageController) : StgItemObject(stageController) {
	typeItem_ = ITEM_USER;
	idImage_ = -1;
	frameWork_ = 0;

	if (auto move = ref_unsync_ptr<StgMovePattern_Item>::Cast(pattern_))
		move->SetItemMoveType(StgMovePattern_Item::MOVE_DOWN);

	bDefaultScoreText_ = true;
}
void StgItemObject_User::SetImageID(int id) {
	idImage_ = id;
	StgItemData* data = _GetItemData();
	if (data) {
		typeItem_ = data->GetItemType();
	}
}
StgItemData* StgItemObject_User::_GetItemData() {
	StgItemData* res = nullptr;
	StgItemManager* itemManager = stageController_->GetItemManager();
	StgItemDataList* dataList = itemManager->GetItemDataList();

	if (dataList) {
		res = dataList->GetData(idImage_);
	}

	return res;
}
void StgItemObject_User::_SetVertexPosition(VERTEX_TLX& vertex, float x, float y, float z, float w) {
	constexpr float bias = -0.5f;
	vertex.position.x = x + bias;
	vertex.position.y = y + bias;
	vertex.position.z = z;
	vertex.position.w = w;
}
void StgItemObject_User::_SetVertexUV(VERTEX_TLX& vertex, float u, float v) {
	vertex.texcoord.x = u;
	vertex.texcoord.y = v;
}
void StgItemObject_User::_SetVertexColorARGB(VERTEX_TLX& vertex, D3DCOLOR color) {
	vertex.diffuse_color = color;
}
void StgItemObject_User::Work() {
	StgItemObject::Work();
	++frameWork_;
}
void StgItemObject_User::RenderOnItemManager() {
	if (!IsVisible()) return;

	StgItemData* itemData = _GetItemData();
	if (itemData == nullptr) return;

	StgItemRenderer* renderer = nullptr;

	BlendMode objBlendType = GetBlendType();
	if (objBlendType == MODE_BLEND_NONE) {
		objBlendType = itemData->GetRenderType();
	}
	renderer = itemData->GetRenderer(objBlendType);

	if (renderer == nullptr) return;

	float scaleX = scale_.x;
	float scaleY = scale_.y;
	float c = 1.0f;
	float s = 0.0f;
	float posy = position_.y;

	StgItemData::AnimationData* frameData = itemData->GetData(frameWork_);

	DxRect<int>* rcSrc = &frameData->rcSrc_;
	DxRect<int>* rcDst = &frameData->rcDest_;
	D3DCOLOR color;

	{
		bool bOutY = false;
		if (position_.y + (rcSrc->bottom - rcSrc->top) / 2 <= 0) {
			bOutY = true;
			rcSrc = itemData->GetOutSrc();
			rcDst = itemData->GetOutDest();
		}

		if (!bOutY) {
			c = cosf(angle_.z);
			s = sinf(angle_.z);
		}
		else {
			scaleX = 1.0;
			scaleY = 1.0;
			posy = (rcSrc->bottom - rcSrc->top) / 2;
		}

		bool bBlendAddRGB = (objBlendType == MODE_BLEND_ADD_RGB);

		color = color_;
		float alphaRate = itemData->GetAlpha() / 255.0f;
		if (bBlendAddRGB) {
			color = ColorAccess::ApplyAlpha(color, alphaRate);
		}
		else {
			byte alpha = ColorAccess::ClampColorRet(((color >> 24) & 0xff) * alphaRate);
			color = (color & 0x00ffffff) | (alpha << 24);
		}
	}

	//if(bIntersected_)color = D3DCOLOR_ARGB(255, 255, 0, 0);//接触テスト

	VERTEX_TLX verts[4];
	int* ptrSrc = reinterpret_cast<int*>(rcSrc);
	int* ptrDst = reinterpret_cast<int*>(rcDst);
	/*
	for (size_t iVert = 0; iVert < 4; iVert++) {
		VERTEX_TLX vt;

		_SetVertexUV(vt, ptrSrc[(iVert & 0b1) << 1] / itemData->GetTextureSize().x, 
			ptrSrc[iVert | 0b1] / itemData->GetTextureSize().y);
		_SetVertexPosition(vt, ptrDst[(iVert & 0b1) << 1], ptrDst[iVert | 0b1]);
		_SetVertexColorARGB(vt, color);

		float px = vt.position.x * scaleX;
		float py = vt.position.y * scaleY;
		vt.position.x = (px * c - py * s) + position_.x;
		vt.position.y = (px * s + py * c) + posy;
		vt.position.z = position_.z;

		verts[iVert] = vt;
	}
	*/
	for (size_t iVert = 0U; iVert < 4U; ++iVert) {
		VERTEX_TLX vt;
		_SetVertexUV(vt, ptrSrc[(iVert & 0b1) << 1], ptrSrc[iVert | 0b1]);
		_SetVertexPosition(vt, ptrDst[(iVert & 0b1) << 1], ptrDst[iVert | 0b1], position_.z);
		_SetVertexColorARGB(vt, color);
		verts[iVert] = vt;
	}
	D3DXVECTOR2 texSizeInv = D3DXVECTOR2(1.0f / itemData->GetTextureSize().x, 1.0f / itemData->GetTextureSize().y);
	DxMath::TransformVertex2D(verts, &D3DXVECTOR2(scaleX, scaleY), &D3DXVECTOR2(c, s), 
		&D3DXVECTOR2(position_.x, posy), &texSizeInv);

	renderer->AddSquareVertex(verts);
}
void StgItemObject_User::Intersect(StgIntersectionTarget* ownTarget, StgIntersectionTarget* otherTarget) {
	if (bDefaultScoreText_)
		_CreateScoreItem();
	stageController_->GetStageInformation()->AddScore(score_);

	gstd::value listValue[2] = {
		DxScript::CreateIntValue(typeItem_),
		DxScript::CreateIntValue(idObject_)
	};
	_NotifyEventToPlayerScript(listValue, 2);
	_NotifyEventToItemScript(listValue, 2);

	auto objectManager = stageController_->GetMainObjectManager();
	objectManager->DeleteObject(this);
}


/**********************************************************
//StgMovePattern_Item
**********************************************************/
StgMovePattern_Item::StgMovePattern_Item(StgMoveObject* target) : StgMovePattern(target) {
	frame_ = 0;
	typeMove_ = MOVE_DOWN;
	speed_ = 0;
	angDirection_ = Math::DegreeToRadian(270);
	posTo_ = D3DXVECTOR2(0, 0);
}
void StgMovePattern_Item::Move() {
	StgItemObject* itemObject = (StgItemObject*)target_;
	StgStageController* stageController = itemObject->GetStageController();

	double px = target_->GetPositionX();
	double py = target_->GetPositionY();
	if (typeMove_ == MOVE_TOPLAYER || (itemObject->IsDefaultCollectionMovement() && itemObject->IsMoveToPlayer())) {
		if (frame_ == 0) speed_ = 6;
		speed_ += 0.075;
		ref_unsync_ptr<StgPlayerObject> objPlayer = stageController->GetPlayerObject();
		if (objPlayer) {
			double angle = atan2(objPlayer->GetY() - py, objPlayer->GetX() - px);
			angDirection_ = angle;
			c_ = cos(angDirection_);
			s_ = sin(angDirection_);
		}
	}
	else if (typeMove_ == MOVE_TOPOSITION_A) {
		double dx = posTo_.x - px;
		double dy = posTo_.y - py;
		speed_ = hypot(dx, dy) / 16.0;

		double angle = atan2(dy, dx);
		angDirection_ = angle;
		if (frame_ == 0) {
			c_ = cos(angDirection_);
			s_ = sin(angDirection_);
		}
		else if (frame_ == 60) {
			speed_ = 0;
			angDirection_ = Math::DegreeToRadian(90);
			typeMove_ = MOVE_DOWN;
			c_ = 0;
			s_ = 1;
		}
	}
	else if (typeMove_ == MOVE_DOWN) {
		speed_ += 3.0 / 60.0;
		if (speed_ > 2.5) speed_ = 2.5;
		angDirection_ = Math::DegreeToRadian(90);
		c_ = 0;
		s_ = 1;
	}
	else if (typeMove_ == MOVE_SCORE) {
		speed_ = 1;
		angDirection_ = Math::DegreeToRadian(270);
		c_ = 0;
		s_ = -1;
	}

	if (typeMove_ != MOVE_NONE) {
		__m128d v1 = Vectorize::MulAdd(
			Vectorize::Replicate(speed_),
			Vectorize::Set(c_, s_),
			Vectorize::Set(px, py));
		target_->SetPositionX(v1.m128d_f64[0]);
		target_->SetPositionY(v1.m128d_f64[1]);
	}

	++frame_;
}

