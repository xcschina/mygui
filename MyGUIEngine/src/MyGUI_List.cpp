/*!
	@file
	@author		Albert Semenov
	@date		11/2007
	@module
*/
#include "MyGUI_Prerequest.h"
#include "MyGUI_Common.h"
#include "MyGUI_List.h"
#include "MyGUI_Button.h"
#include "MyGUI_VScroll.h"
#include "MyGUI_WidgetOIS.h"
#include "MyGUI_WidgetSkinInfo.h"

namespace MyGUI
{

	MYGUI_RTTI_CHILD_IMPLEMENT( List, Widget );

	List::List(const IntCoord& _coord, Align _align, const WidgetSkinInfoPtr _info, WidgetPtr _parent, ICroppedRectangle * _croppedParent, IWidgetCreator * _creator, const std::string & _name) :
		Widget(_coord, _align, _info, _parent, _croppedParent, _creator, _name),
		mWidgetScroll(null),
		mTopIndex(0),
		mOffsetTop(0),
		mRangeIndex(-1),
		mLastRedrawLine(0),
		mIndexSelect(ITEM_NONE),
		mLineActive(ITEM_NONE),
		mIsFocus(false),
		mNeedVisibleScroll(true)
	{
		initialiseWidgetSkin(_info);
	}

	List::~List()
	{
		shutdownWidgetSkin();
	}

	void List::baseChangeWidgetSkin(WidgetSkinInfoPtr _info)
	{
		shutdownWidgetSkin();
		Widget::baseChangeWidgetSkin(_info);
		initialiseWidgetSkin(_info);
	}

	void List::initialiseWidgetSkin(WidgetSkinInfoPtr _info)
	{
		// нам нужен фокус клавы
		mNeedKeyFocus = true;

		for (VectorWidgetPtr::iterator iter=mWidgetChildSkin.begin(); iter!=mWidgetChildSkin.end(); ++iter) {
			if (*(*iter)->_getInternalData<std::string>() == "VScroll") {
				MYGUI_DEBUG_ASSERT( ! mWidgetScroll, "widget already assigned");
				mWidgetScroll = (*iter)->castType<VScroll>();
				mWidgetScroll->eventScrollChangePosition = newDelegate(this, &List::notifyScrollChangePosition);
				mWidgetScroll->eventMouseButtonPressed = newDelegate(this, &List::notifyMousePressed);
			}
			else if (*(*iter)->_getInternalData<std::string>() == "Client") {
				MYGUI_DEBUG_ASSERT( ! mWidgetClient, "widget already assigned");
				mWidgetClient = (*iter);
				mWidgetClient->eventMouseButtonPressed = newDelegate(this, &List::notifyMousePressed);
			}
		}
		MYGUI_ASSERT(null != mWidgetScroll, "Child VScroll not found in skin (List must have VScroll)");
		MYGUI_ASSERT(null != mWidgetClient, "Child Widget Client not found in skin (List must have Client)");

		// парсим свойства
		const MapString & properties = _info->getProperties();
		MapString::const_iterator iterS = properties.find("SkinLine");
		if (iterS != properties.end()) mSkinLine = iterS->second;
		MYGUI_ASSERT(false == mSkinLine.empty(), "SkinLine property not found (List must have SkinLine property)");

		iterS = properties.find("HeightLine");
		if (iterS != properties.end()) mHeightLine = utility::parseInt(iterS->second);
		if (mHeightLine < 1) mHeightLine = 1;


		mWidgetScroll->setScrollPage((size_t)mHeightLine);
		mWidgetScroll->setScrollViewPage((size_t)mHeightLine);

		updateScroll();
		updateLine();

	}

	void List::shutdownWidgetSkin()
	{
		mWidgetScroll = null;
		mWidgetClient = null;
	}

	void List::onMouseWheel(int _rel)
	{
		notifyMouseWheel(null, _rel);

		// !!! ОБЯЗАТЕЛЬНО вызывать в конце метода
		Widget::onMouseWheel(_rel);
	}

	void List::onKeySetFocus(WidgetPtr _old)
	{
		mIsFocus = true;
		_updateState();

		// !!! ОБЯЗАТЕЛЬНО вызывать в конце метода
		Widget::onKeySetFocus(_old);
	}

	void List::onKeyLostFocus(WidgetPtr _new)
	{
		mIsFocus = false;
		_updateState();

		// !!! ОБЯЗАТЕЛЬНО вызывать в конце метода
		Widget::onKeyLostFocus(_new);
	}

	void List::onKeyButtonPressed(KeyCode _key, Char _char)
	{
		// очень секретный метод, запатентованный механизм движения курсора
		if (getItemCount() == 0) {

			// !!! ОБЯЗАТЕЛЬНО вызывать в конце метода
			Widget::onKeyButtonPressed(_key, _char);
			return;
		}

		size_t sel = mIndexSelect;

		if (_key == KC_UP) {

			if (sel != 0) {
				if (sel == ITEM_NONE) sel = 0;
				else sel --;
			}

		} else if (_key == KC_DOWN) {

			if (sel == ITEM_NONE) sel = 0;
			else sel ++;

			if (sel >= getItemCount()) {
				// старое значение
				sel = mIndexSelect;
			}

		} else if (_key == KC_HOME) {

			if (sel != 0) sel = 0;

		} else if (_key == KC_END) {

			if (sel != (getItemCount() - 1)) {
				sel = getItemCount() - 1;
			}

		} else if (_key == KC_PGUP) {

			if (sel != 0) {
				if (sel == ITEM_NONE) sel = 0;
				else {
					size_t page = mWidgetClient->getHeight() / mHeightLine;
					if (sel <= page) sel = 0;
					else sel -= page;
				}
			}

		} else if (_key == KC_PGDOWN) {

			if (sel != (getItemCount() - 1)) {
				if (sel == ITEM_NONE) sel = 0;
				else {
					sel += mWidgetClient->getHeight() / mHeightLine;
					if (sel >= getItemCount()) sel = getItemCount() - 1;
				}
			}

		} else if ((_key == KC_RETURN) || (_key == KC_NUMPADENTER)) {
			if (sel != ITEM_NONE) {
				eventListSelectAccept(this, sel);

				Widget::onKeyButtonPressed(_key, _char);
				// выходим, так как изменили колличество строк
				return;
			}

		}

		if (sel != mIndexSelect) {
			if ( false == isItemVisibleAt(sel)) {
				beginToItemAt(sel);
				_sendEventChangeScroll(mWidgetScroll->getScrollPosition());
			}
			setItemSelectedAt(sel);
			// изменилась позиция
			eventListChangePosition(this, mIndexSelect);
		}

		// !!! ОБЯЗАТЕЛЬНО вызывать в конце метода
		Widget::onKeyButtonPressed(_key, _char);
	}

	void List::notifyMouseWheel(WidgetPtr _sender, int _rel)
	{
		if (mRangeIndex <= 0) return;

		int offset = (int)mWidgetScroll->getScrollPosition();
		if (_rel < 0) offset += mHeightLine;
		else  offset -= mHeightLine;

		if (offset >= mRangeIndex) offset = mRangeIndex;
		else if (offset < 0) offset = 0;

		if ((int)mWidgetScroll->getScrollPosition() == offset) return;

		mWidgetScroll->setScrollPosition(offset);
		_setScrollView(offset);
		_sendEventChangeScroll(offset);
	}

	void List::notifyScrollChangePosition(WidgetPtr _sender, size_t _position)
	{
		_setScrollView(_position);
		_sendEventChangeScroll(_position);
	}

	void List::notifyMousePressed(WidgetPtr _sender, int _left, int _top, MouseButton _id)
	{
		if (MB_Left != _id) return;

		if (_sender == mWidgetScroll) return;

		// если выделен клиент, то сбрасываем
		if (_sender == mWidgetClient) {

			if (mIndexSelect != ITEM_NONE) {
				_selectIndex(mIndexSelect, false);
				mIndexSelect = ITEM_NONE;
				eventListChangePosition(this, mIndexSelect);
			}
			eventListMouseItemActivate(this, mIndexSelect);

		// если не клиент, то просчитывам
		}
		else {
			size_t index = *_sender->_getInternalData<size_t>() + mTopIndex;

			if (mIndexSelect != index) {
				_selectIndex(mIndexSelect, false);
				_selectIndex(index, true);
				mIndexSelect = index;
				eventListChangePosition(this, mIndexSelect);
			}
			eventListMouseItemActivate(this, mIndexSelect);

		}
	}

	void List::notifyMouseDoubleClick(WidgetPtr _sender)
	{
		if (mIndexSelect != ITEM_NONE)
			eventListSelectAccept(this, mIndexSelect);
	}

	void List::setPosition(const IntPoint & _point)
	{
		Widget::setPosition(_point);
	}

	void List::setSize(const IntSize & _size)
	{
		Widget::setSize(_size);
		updateScroll();
		updateLine();
	}

	void List::setCoord(const IntCoord & _coord)
	{
		Widget::setCoord(_coord);
		updateScroll();
		updateLine();
	}

	void List::updateScroll()
	{
		mRangeIndex = (mHeightLine * (int)mItemsInfo.size()) - mWidgetClient->getHeight();

		if ( (false == mNeedVisibleScroll) || (mRangeIndex < 1) || (mWidgetScroll->getLeft() <= mWidgetClient->getLeft()) )
		{
			if (mWidgetScroll->isShow()) {
				mWidgetScroll->hide();
				// увеличиваем клиентскую зону на ширину скрола
				mWidgetClient->setSize(mWidgetClient->getWidth() + mWidgetScroll->getWidth(), mWidgetClient->getHeight());
			}
		}
		else if (false == mWidgetScroll->isShow())
		{
			mWidgetClient->setSize(mWidgetClient->getWidth() - mWidgetScroll->getWidth(), mWidgetClient->getHeight());
			mWidgetScroll->show();
		}

		mWidgetScroll->setScrollRange(mRangeIndex + 1);
		if ((int)mItemsInfo.size()) mWidgetScroll->setTrackSize( mWidgetScroll->getLineSize() * mWidgetClient->getHeight() / mHeightLine / (int)mItemsInfo.size() );
	}

	void List::updateLine(bool _reset)
	{
		// сбрасываем
		if (_reset) {
			mOldSize.clear();
			mLastRedrawLine = 0;
		}

		// позиция скролла
		int position = mTopIndex * mHeightLine + mOffsetTop;

		// если высота увеличивалась то добавляем виджеты
		if (mOldSize.height < mCoord.height) {

			int height = (int)mWidgetLines.size() * mHeightLine - mOffsetTop;

			// до тех пор, пока не достигнем максимального колличества, и всегда на одну больше
			while ( (height <= (mWidgetClient->getHeight() + mHeightLine)) && (mWidgetLines.size() < mItemsInfo.size()) ) {
				// создаем линию
				WidgetPtr line = mWidgetClient->createWidgetT("Button", mSkinLine, 0, height, mWidgetClient->getWidth(), mHeightLine, Align::Top | Align::HStretch);
				// подписываемся на всякие там события
				line->eventMouseButtonPressed = newDelegate(this, &List::notifyMousePressed);
				line->eventMouseButtonDoubleClick = newDelegate(this, &List::notifyMouseDoubleClick);
				line->eventMouseWheel = newDelegate(this, &List::notifyMouseWheel);
				line->eventMouseSetFocus = newDelegate(this, &List::notifyMouseSetFocus);
				line->eventMouseLostFocus = newDelegate(this, &List::notifyMouseLostFocus);
				// присваиваем порядковый номер, длу простоты просчета
				line->_setInternalData((size_t)mWidgetLines.size());
				// и сохраняем
				mWidgetLines.push_back(line);
				height += mHeightLine;
			};

			// проверяем на возможность не менять положение списка
			if (position >= mRangeIndex) {

				// размер всех помещается в клиент
				if (mRangeIndex <= 0) {

					// обнуляем, если надо
					if (position || mOffsetTop || mTopIndex) {

						position = 0;
						mTopIndex = 0;
						mOffsetTop = 0;
						mLastRedrawLine = 0; // чтобы все перерисовалось

						// выравниваем
						int offset = 0;
						for (size_t pos=0; pos<mWidgetLines.size(); pos++) {
							mWidgetLines[pos]->setPosition(0, offset);
							offset += mHeightLine;
						}
					}

				} else {

					// прижимаем список к нижней границе
					int count = mWidgetClient->getHeight() / mHeightLine;
					mOffsetTop = mHeightLine - (mWidgetClient->getHeight() % mHeightLine);

					if (mOffsetTop == mHeightLine) {
						mOffsetTop = 0;
						count --;
					}

					int top = (int)mItemsInfo.size() - count - 1;

					// выравниваем
					int offset = 0 - mOffsetTop;
					for (size_t pos=0; pos<mWidgetLines.size(); pos++) {
						mWidgetLines[pos]->setPosition(0, offset);
						offset += mHeightLine;
					}

					// высчитываем положение, должно быть максимальным
					position = top * mHeightLine + mOffsetTop;

					// если индех изменился, то перерисовываем линии
					if (top != mTopIndex) {
						mTopIndex = top;
						_redrawItemRange();
					}

				}
			}

			// увеличился размер, но прокрутки вниз небыло, обновляем линии снизу
			_redrawItemRange(mLastRedrawLine);

		} // if (old_cy < mCoord.height)

		// просчитываем положение скролла
		mWidgetScroll->setScrollPosition(position);

		mOldSize.width = mCoord.width;
		mOldSize.height = mCoord.height;
	}

	void List::_redrawItemRange(size_t _start)
	{
		// перерисовываем линии, только те, что видны
		size_t pos = _start;
		for (; pos<mWidgetLines.size(); pos++) {
			// индекс в нашем массиве
			size_t index = pos + (size_t)mTopIndex;

			// не будем заходить слишком далеко
			if (index >= mItemsInfo.size()) {
				// запоминаем последнюю перерисованную линию
				mLastRedrawLine = pos;
				break;
			}
			if (mWidgetLines[pos]->getTop() > mWidgetClient->getHeight()) {
				// запоминаем последнюю перерисованную линию
				mLastRedrawLine = pos;
				break;
			}

			// если был скрыт, то покажем
			if (false == mWidgetLines[pos]->isShow()) mWidgetLines[pos]->show();
			// обновляем текст
			mWidgetLines[pos]->setCaption(mItemsInfo[index].first);

			// если нужно выделить ,то выделим
			if (index == mIndexSelect) {
				if (!static_cast<ButtonPtr>(mWidgetLines[pos])->getButtonPressed())
					static_cast<ButtonPtr>(mWidgetLines[pos])->setButtonPressed(true);
			} else {
				if (static_cast<ButtonPtr>(mWidgetLines[pos])->getButtonPressed())
					static_cast<ButtonPtr>(mWidgetLines[pos])->setButtonPressed(false);
			}
		}

		// если цикл весь прошли, то ставим максимальную линию
		if (pos >= mWidgetLines.size()) mLastRedrawLine = pos;
	}

	// перерисовывает индекс
	void List::_redrawItem(size_t _index)
	{
		// невидно
		if (_index < (size_t)mTopIndex) return;
		_index -= (size_t)mTopIndex;
		// тоже невидно
		if (_index >= mLastRedrawLine) return;

		MYGUI_ASSERT_RANGE(_index, mItemsInfo.size(), "List::_redrawItem");
		// перерисовываем
		mWidgetLines[_index]->setCaption(mItemsInfo[_index + mTopIndex].first);
	}

	void List::insertItemAt(size_t _index, const Ogre::UTFString & _name, Any _data)
	{
		MYGUI_ASSERT_RANGE_INSERT(_index, mItemsInfo.size(), "List::insertItemAt");
		if (_index == ITEM_NONE) _index = mItemsInfo.size();

		// вставляем физически
		mItemsInfo.insert(mItemsInfo.begin() + _index, PairItem(_name, _data));

		// если надо, то меняем выделенный элемент
		if ( (mIndexSelect != ITEM_NONE) && (_index <= mIndexSelect) ) mIndexSelect++;

		// строка, до первого видимого элемента
		if ( (_index <= (size_t)mTopIndex) && (mRangeIndex > 0) ) {
			mTopIndex ++;
			// просчитываем положение скролла
			mWidgetScroll->setScrollRange(mWidgetScroll->getScrollRange() + mHeightLine);
			if ((int)mItemsInfo.size()) mWidgetScroll->setTrackSize( mWidgetScroll->getLineSize() * mWidgetClient->getHeight() / mHeightLine / (int)mItemsInfo.size() );
			mWidgetScroll->setScrollPosition(mTopIndex * mHeightLine + mOffsetTop);
			mRangeIndex += mHeightLine;

		}
		else {

			// высчитывам положение строки
			int offset = ((int)_index - mTopIndex) * mHeightLine - mOffsetTop;

			// строка, после последнего видимого элемента, плюс одна строка (потому что для прокрутки нужно на одну строчку больше)
			if (mWidgetClient->getHeight() < (offset - mHeightLine)) {
				// просчитываем положение скролла
				mWidgetScroll->setScrollRange(mWidgetScroll->getScrollRange() + mHeightLine);
				if ((int)mItemsInfo.size()) mWidgetScroll->setTrackSize( mWidgetScroll->getLineSize() * mWidgetClient->getHeight() / mHeightLine / (int)mItemsInfo.size() );
				mWidgetScroll->setScrollPosition(mTopIndex * mHeightLine + mOffsetTop);
				mRangeIndex += mHeightLine;

			// строка в видимой области
			} else {

				// обновляем все
				updateScroll();
				updateLine(true);

				// позже сюда еще оптимизацию по колличеству перерисовок
			}
		}
	}

	void List::removeItemAt(size_t _index)
	{
		MYGUI_ASSERT_RANGE(_index, mItemsInfo.size(), "List::removeItemAt");

		// удяляем физически строку
		_deleteString(_index);

		// если надо, то меняем выделенный элемент
		if (mItemsInfo.empty()) mIndexSelect = ITEM_NONE;
		else if (mIndexSelect != ITEM_NONE) {
			if (_index < mIndexSelect) mIndexSelect--;
			else if ( (_index == mIndexSelect) && (mIndexSelect == (mItemsInfo.size())) ) mIndexSelect--;
		}

		// если виджетов стало больше , то скрываем крайний
		if (mWidgetLines.size() > mItemsInfo.size()) {
			mWidgetLines[mItemsInfo.size()]->hide();
		}

		// строка, до первого видимого элемента
		if (_index < (size_t)mTopIndex) {
			mTopIndex --;
			// просчитываем положение скролла
			mWidgetScroll->setScrollRange(mWidgetScroll->getScrollRange() - mHeightLine);
			if ((int)mItemsInfo.size()) mWidgetScroll->setTrackSize( mWidgetScroll->getLineSize() * mWidgetClient->getHeight() / mHeightLine / (int)mItemsInfo.size() );
			mWidgetScroll->setScrollPosition(mTopIndex * mHeightLine + mOffsetTop);
			mRangeIndex -= mHeightLine;

		}
		else {

			// высчитывам положение удаляемой строки
			int offset = ((int)_index - mTopIndex) * mHeightLine - mOffsetTop;

			// строка, после последнего видимого элемента
			if (mWidgetClient->getHeight() < offset) {
				// просчитываем положение скролла
				mWidgetScroll->setScrollRange(mWidgetScroll->getScrollRange() - mHeightLine);
				if ((int)mItemsInfo.size()) mWidgetScroll->setTrackSize( mWidgetScroll->getLineSize() * mWidgetClient->getHeight() / mHeightLine / (int)mItemsInfo.size() );
				mWidgetScroll->setScrollPosition(mTopIndex * mHeightLine + mOffsetTop);
				mRangeIndex -= mHeightLine;

			// строка в видимой области
			}
			else {

				// обновляем все
				updateScroll();
				updateLine(true);

				// позже сюда еще оптимизацию по колличеству перерисовок
			}
		}
	}

	void List::_deleteString(size_t _index)
	{
		for (size_t pos=_index+1; pos<mItemsInfo.size(); pos++) {
			mItemsInfo[pos-1] = mItemsInfo[pos];
		}
		mItemsInfo.pop_back();
	}

	void List::setItemSelectedAt(size_t _index)
	{
		MYGUI_ASSERT_RANGE_AND_NONE(_index, mItemsInfo.size(), "List::setItemSelectedAt");
		if (mIndexSelect != _index) {
			_selectIndex(mIndexSelect, false);
			_selectIndex(_index, true);
			mIndexSelect = _index;
		}
	}

	void List::_selectIndex(size_t _index, bool _select)
	{
		if (_index == ITEM_NONE) return;
		// не видно строки
		if (_index < (size_t)mTopIndex) return;
		// высчитывам положение строки
		int offset = ((int)_index - mTopIndex) * mHeightLine - mOffsetTop;
		// строка, после последнего видимого элемента
		if (mWidgetClient->getHeight() < offset) return;

		static_cast<ButtonPtr>(mWidgetLines[_index-mTopIndex])->setButtonPressed(_select);
	}

	void List::beginToItemAt(size_t _index)
	{
		MYGUI_ASSERT_RANGE(_index, mItemsInfo.size(), "List::beginToItemAt");
		if (mRangeIndex <= 0) return;

		int offset = (int)_index * mHeightLine;
		if (offset >= mRangeIndex) offset = mRangeIndex;

		if ((int)mWidgetScroll->getScrollPosition() == offset) return;

		mWidgetScroll->setScrollPosition(offset);
		notifyScrollChangePosition(null, offset);
	}

	// видим ли мы элемент, полностью или нет
	bool List::isItemVisibleAt(size_t _index, bool _fill)
	{
		// если элемента нет, то мы его не видим (в том числе когда их вообще нет)
		if (_index >= mItemsInfo.size()) return false;
		// если скрола нет, то мы палюбак видим
		if (mRangeIndex <= 0) return true;

		// строка, до первого видимого элемента
		if (_index < (size_t)mTopIndex) return false;

		// строка это верхний выделенный
		if (_index == (size_t)mTopIndex) {
			if ( (mOffsetTop != 0) && (_fill) ) return false; // нам нужна полностью видимость
			return true;
		}

		// высчитывам положение строки
		int offset = ((int)_index - mTopIndex) * mHeightLine - mOffsetTop;

		// строка, после последнего видимого элемента
		if (mWidgetClient->getHeight() < offset) return false;

		// если мы внизу и нам нужен целый
		if ((mWidgetClient->getHeight() < (offset + mHeightLine)) && (_fill) ) return false;

		return true;
	}

	void List::removeAllItems()
	{
		mTopIndex = 0;
		mIndexSelect = ITEM_NONE;
		mOffsetTop = 0;

		mItemsInfo.clear();

		for (size_t pos=0; pos<mWidgetLines.size(); pos++)
			mWidgetLines[pos]->hide();

		// обновляем все
		updateScroll();
		updateLine(true);
	}

	void List::setItemNameAt(size_t _index, const Ogre::UTFString & _name)
	{
		MYGUI_ASSERT_RANGE(_index, mItemsInfo.size(), "List::setItemNameAt");
		mItemsInfo[_index].first =_name;
		_redrawItem(_index);
	}

	void List::setItemDataAt(size_t _index, Any _data)
	{
		MYGUI_ASSERT_RANGE(_index, mItemsInfo.size(), "List::setItemDataAt");
		mItemsInfo[_index].second = _data;
		_redrawItem(_index);
	}

	const Ogre::UTFString & List::getItemNameAt(size_t _index)
	{
		MYGUI_ASSERT_RANGE(_index, mItemsInfo.size(), "List::getItemNameAt");
		return mItemsInfo[_index].first;
	}

	void List::notifyMouseSetFocus(WidgetPtr _sender, WidgetPtr _old)
	{
		mLineActive = *_sender->_getInternalData<size_t>();
		eventListMouseItemFocus(this, mLineActive + (size_t)mTopIndex);
	}

	void List::notifyMouseLostFocus(WidgetPtr _sender, WidgetPtr _new)
	{
		if ((null == _new) || (_new->getParent() != mWidgetClient)) {
			mLineActive = ITEM_NONE;
			eventListMouseItemFocus(this, ITEM_NONE);
		}
	}

	void List::_setItemFocus(size_t _position, bool _focus)
	{
		size_t index = (_position - mTopIndex);
		if (index < mWidgetLines.size())
			static_cast<ButtonPtr>(mWidgetLines[index])->_setMouseFocus(_focus);
	}	

	void List::setScrollVisible(bool _visible)
	{
		if (mNeedVisibleScroll == _visible) return;
		mNeedVisibleScroll = _visible;
		updateScroll();
	}

	void List::setScrollPosition(size_t _position)
	{
		if (mWidgetScroll->getScrollRange() > _position) {
			mWidgetScroll->setScrollPosition(_position);
			_setScrollView(_position);
		}
	}

	void List::_setScrollView(size_t _position)
	{
		mOffsetTop = ((int)_position % mHeightLine);

		// смещение с отрицательной стороны
		int offset = 0 - mOffsetTop;

		for (size_t pos=0; pos<mWidgetLines.size(); pos++) {
			mWidgetLines[pos]->setPosition(IntPoint(0, offset));
			offset += mHeightLine;
		}

		// если индех изменился, то перерисовываем линии
		int top = ((int)_position / mHeightLine);
		if (top != mTopIndex) {
			mTopIndex = top;
			_redrawItemRange();
		}

		// прорисовываем все нижние строки, если они появились
		_redrawItemRange(mLastRedrawLine);
	}

	void List::_sendEventChangeScroll(size_t _position)
	{
		eventListChangeScroll(this, _position);
		if (ITEM_NONE != mLineActive) eventListMouseItemFocus(this, mLineActive + (size_t)mTopIndex);
	}

	void List::swapItemsAt(size_t _index1, size_t _index2)
	{
		MYGUI_ASSERT_RANGE(_index1, mItemsInfo.size(), "List::swapItemsAt");
		MYGUI_ASSERT_RANGE(_index2, mItemsInfo.size(), "List::swapItemsAt");

		if (_index1 == _index2) return;

		std::swap(mItemsInfo[_index1], mItemsInfo[_index2]);

		_redrawItem(_index1);
		_redrawItem(_index2);
	}

} // namespace MyGUI
