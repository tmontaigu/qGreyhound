#include <QFutureWatcher>
#include <QEventLoop>
#include <QtConcurrent>
#include <QThreadPool>

#include <DgmOctree.h>

#include <GreyhoundReader.hpp>

#include "GreyhoundDownloader.h"

void
download_and_convert_cloud(ccPointCloud *cloud, const pdal::Options& opts, PDALConverter converter)
{
	std::exception_ptr eptr(nullptr);
	pdal::PointTable table;
	pdal::GreyhoundReader reader;

	reader.addOptions(opts);

	reader.prepare(table);
	pdal::PointViewSet view_set = reader.execute(table);
	const pdal::PointViewPtr view_ptr = *view_set.begin();
	converter.convert(view_ptr, table.layout(), cloud);
}

void
download_and_convert_cloud_threaded(ccPointCloud *cloud, const pdal::Options& opts, const PDALConverter converter)
{
	std::exception_ptr eptr(nullptr);
	const auto dl = [&cloud, &eptr](const pdal::Options opts, const PDALConverter converter) {
		try {
			download_and_convert_cloud(cloud, opts, converter);
		}
		catch (...) {
			eptr = std::current_exception();
		}
	};

	QFutureWatcher<void> downloader;
	QEventLoop loop;
	downloader.setFuture(QtConcurrent::run(dl, opts, converter));
	QObject::connect(&downloader, &QFutureWatcher<void>::finished, &loop, &QEventLoop::quit);
	loop.exec();
	downloader.waitForFinished();

	if (eptr) {
		std::rethrow_exception(eptr);
	}
}

GreyhoundDownloader::GreyhoundDownloader(const pdal::Options& opts, const uint32_t start_depth, const pdal::greyhound::Bounds bounds, const PDALConverter converter)
	: m_opts(opts)
	, m_current_depth(start_depth)
	, m_bounds(bounds)
	, m_converter(converter)
{
}

class DlWorker : public QRunnable
{
public:
	DlWorker(std::function<void(BoundsDepth)> f);
	~DlWorker();
	void run() override;

	BoundsDepth m;
private:
	std::function<void(BoundsDepth)> m_f;

};

DlWorker::DlWorker(std::function<void(BoundsDepth)> f)
	: QRunnable()
	, m_f(f)
{
}

DlWorker::~DlWorker()
{
}

void DlWorker::run()
{
	m_f(m);
}

void
GreyhoundDownloader::download_to(ccPointCloud *cloud, const DownloadMethod method)
{
	std::queue<BoundsDepth> qin;
	std::queue<BoundsDepth> qout;

	std::mutex mu_qout;

	using mutex_locker = std::lock_guard<std::mutex>;


	QThreadPool pool;
	pool.setMaxThreadCount(8);


	const auto f = [&qout, &mu_qout, this](BoundsDepth m) {
		pdal::Options opts(m_opts);
		opts.add("depth_begin", m.depth);
		opts.add("depth_end", m.depth + 1);
		opts.add("bounds", m.b.toJson());
		try {
			m.cloud = new ccPointCloud("a");
			download_and_convert_cloud(m.cloud, opts, m_converter);
		}
		catch (const std::exception& e) {
			ccLog::Print(QString("[qGreyhound] %1").arg(e.what()));
			m.cloud = nullptr;
		}

		mutex_locker lk(mu_qout);
		qout.push(m);
	};

	qin.emplace(m_bounds, static_cast<int>(m_current_depth));
	while (!qin.empty() || !qout.empty() || pool.activeThreadCount() != 0)
	{
		if (!qin.empty())
		{
			BoundsDepth m = qin.front();
			qin.pop();
			auto w = new DlWorker(f);
			w->setAutoDelete(true);
			w->m = m;
			pool.start(w);
		}
		BoundsDepth m;
		{
			mutex_locker lk(mu_qout);
			if (qout.empty()) {
				continue;
			}
			m = qout.front();
			qout.pop();
		}
		if (m.cloud && m.cloud->size() && cloud)
		{
			if (cloud->hasDisplayedScalarField() || cloud->colorsShown()) {
				m.cloud->showSF(false);
				m.cloud->showColors(false);
			}

			cloud->append(m.cloud, cloud->size());
			//Ideally we would like to refresh soon after appending
			//but we can't because the main thread also refreshes the 
			//display from time to time/ on some user action causing crashes?
			//cloud->prepareDisplayForRefresh();
			//cloud->refreshDisplay();

			if (m.depth + 1 <= CCLib::DgmOctree::MAX_OCTREE_LEVEL)
			{
				switch (method)
				{
				case GreyhoundDownloader::DEPTH_BY_DEPTH:
					qin.emplace(m.b, m.depth + 1);
					break;
				case GreyhoundDownloader::QUADTREE:
					qin.emplace(m.b.getSe(), m.depth + 1);
					qin.emplace(m.b.getSw(), m.depth + 1);
					qin.emplace(m.b.getNe(), m.depth + 1);
					qin.emplace(m.b.getNw(), m.depth + 1);
					break;
				case GreyhoundDownloader::OCTREE:
					break;
				default:
					break;
				}
			}
		}
	}
}